/*
 * usb_driver.c - USB Mass Storage Driver
 */

#include <sysclib.h>
#include <stdarg.h>
#include <tamtypes.h>
#include <thsemap.h>
#include <thbase.h>
#include <stdio.h>
#include <usbd.h>
#include <usbd_macro.h>
#include <errno.h>

//#define DEBUG  //comment out this line when not debugging

#include "mass_debug.h"
#include "usbhd_common.h"
#include "mass_stor.h"
#include "scache.h"

#define getBI32(__buf) ((((u8 *) (__buf))[3] << 0) | (((u8 *) (__buf))[2] << 8) | (((u8 *) (__buf))[1] << 16) | (((u8 *) (__buf))[0] << 24))

#define USB_SUBCLASS_MASS_RBC		0x01
#define USB_SUBCLASS_MASS_ATAPI		0x02
#define USB_SUBCLASS_MASS_QIC		0x03
#define USB_SUBCLASS_MASS_UFI		0x04
#define USB_SUBCLASS_MASS_SFF_8070I 	0x05
#define USB_SUBCLASS_MASS_SCSI		0x06

#define USB_PROTOCOL_MASS_CBI		0x00
#define USB_PROTOCOL_MASS_CBI_NO_CCI	0x01
#define USB_PROTOCOL_MASS_BULK_ONLY	0x50

#define TAG_TEST_UNIT_READY     0
#define TAG_REQUEST_SENSE	3
#define TAG_INQUIRY		18
#define TAG_READ_CAPACITY       37
#define TAG_READ		40
#define TAG_START_STOP_UNIT	33
#define TAG_WRITE		42

#define DEVICE_DETECTED		1
#define DEVICE_CONFIGURED	2

int fat_connect(mass_dev* dev);
int fat_disconnect(mass_dev* dev);

typedef struct _cbw_packet {
	unsigned int signature;
	unsigned int tag;
	unsigned int dataTransferLength;
	unsigned char flags;
	unsigned char lun;
	unsigned char comLength;		//command data length
	unsigned char comData[16];		//command data
} cbw_packet;

typedef struct _csw_packet {
	unsigned int signature;
	unsigned int tag;
	unsigned int dataResidue;
	unsigned char status;
} csw_packet;

typedef struct _sense_data {
    u8 error_code;
    u8 res1;
    u8 sense_key;
    u8 information[4];
    u8 add_sense_len;
    u8 res3[4];
    u8 add_sense_code;
    u8 add_sense_qual;
    u8 res4[4];
} sense_data __attribute__((packed));

//void usb_bulk_read1(int resultCode, int bytes, void* arg);
//void usb_bulk_read2(int resultCode, int bytes, void* arg);

static UsbDriver driver;

volatile int wait_for_connect = 1;

typedef struct _usb_callback_data {
    int semh;
    int returnCode;
    int returnSize;
} usb_callback_data;

static int residue;

#define NUM_DEVICES 5
static mass_dev g_mass_device[NUM_DEVICES];


inline void initCBWPacket(cbw_packet* packet)
{
	packet->signature = 0x43425355;
	packet->flags = 0x80; //80->data in,  00->out
	packet->lun = 0;
}

inline void initCSWPacket(csw_packet* packet)
{
	packet->signature = 0x53425355;
}

inline void cbw_scsi_test_unit_ready(cbw_packet* packet)
{
	packet->tag = TAG_TEST_UNIT_READY;
	packet->dataTransferLength = 0;		//TUR_REPLY_LENGTH
	packet->flags = 0x80;			//data will flow In
	packet->comLength = 6;			//short inquiry command

	//scsi command packet
	packet->comData[0] = 0x00;		//TUR operation code
	packet->comData[1] = 0;			//lun/reserved
	packet->comData[2] = 0;			//reserved
	packet->comData[3] = 0;			//reserved
	packet->comData[4] = 0;			//reserved
	packet->comData[5] = 0;			//control
}

inline void cbw_scsi_request_sense(cbw_packet* packet)
{
	packet->tag = - TAG_REQUEST_SENSE;
	packet->dataTransferLength = 18	;	//REQUEST_SENSE_REPLY_LENGTH
	packet->flags = 0x80;			//sense data will flow In
	packet->comLength = 6;			//scsi command of size 6

	//scsi command packet
	packet->comData[0] = 0x03;		//request sense operation code
	packet->comData[1] = 0;			//lun/reserved
	packet->comData[2] = 0;			//reserved
	packet->comData[3] = 0;			//reserved
	packet->comData[4] = 18;		//allocation length
	packet->comData[5] = 0;			//Control

}

inline void cbw_scsi_inquiry(cbw_packet* packet)
{
	packet->tag = - TAG_INQUIRY;
	packet->dataTransferLength = 36;	//INQUIRY_REPLY_LENGTH
	packet->flags = 0x80;			//inquiry data will flow In
	packet->comLength = 6;			//short inquiry command

	//scsi command packet
	packet->comData[0] = 0x12;		//inquiry operation code
	packet->comData[1] = 0;			//lun/reserved
	packet->comData[2] = 0;			//page code
	packet->comData[3] = 0;			//reserved
	packet->comData[4] = 36;		//inquiry reply length
	packet->comData[5] = 0;			//reserved/flag/link/
}

inline void cbw_scsi_start_stop_unit(cbw_packet* packet)
{
	packet->tag = - TAG_START_STOP_UNIT;
	packet->dataTransferLength = 0;	//START_STOP_REPLY_LENGTH
	packet->flags = 0x80;			//inquiry data will flow In
	packet->comLength = 6;			//short SCSI command

	//scsi command packet
	packet->comData[0] = 0x1B;		//start stop unit operation code
	packet->comData[1] = 1;			//lun/reserved/immed
	packet->comData[2] = 0;			//reserved
	packet->comData[3] = 0;			//reserved
	packet->comData[4] = 1;			//reserved/LoEj/Start "Start the media and acquire the format type"
	packet->comData[5] = 0;			//control
}

inline void cbw_scsi_read_capacity(cbw_packet* packet)
{
	packet->tag = - TAG_READ_CAPACITY;
	packet->dataTransferLength = 8	;	//READ_CAPACITY_REPLY_LENGTH
	packet->flags = 0x80;			//inquiry data will flow In
	packet->comLength = 10;			//scsi command of size 10

	//scsi command packet
	packet->comData[0] = 0x25;		//read capacity operation code
	packet->comData[1] = 0;			//lun/reserved/RelAdr
	packet->comData[2] = 0;			//LBA 1
	packet->comData[3] = 0;			//LBA 2
	packet->comData[4] = 0;			//LBA 3
	packet->comData[5] = 0;			//LBA 4
	packet->comData[6] = 0;			//Reserved
	packet->comData[7] = 0;			//Reserved
	packet->comData[8] = 0;			//Reserved
	packet->comData[9] = 0;			//Control

}

inline void cbw_scsi_read_sector(cbw_packet* packet, int lba, int sectorSize, int sectorCount)
{
	packet->tag = - TAG_READ;
	packet->dataTransferLength = sectorSize	 * sectorCount;
	packet->flags = 0x80;			//read data will flow In
	packet->comLength = 10;			//scsi command of size 10

	//scsi command packet
	packet->comData[0] = 0x28;		//read operation code
	packet->comData[1] = 0;			//LUN/DPO/FUA/Reserved/Reldr
	packet->comData[2] = (lba & 0xFF000000) >> 24;	//lba 1 (MSB)
	packet->comData[3] = (lba & 0xFF0000) >> 16;	//lba 2
	packet->comData[4] = (lba & 0xFF00) >> 8;	//lba 3
	packet->comData[5] = (lba & 0xFF);		//lba 4 (LSB)
	packet->comData[6] = 0;			//Reserved
	packet->comData[7] = (sectorCount & 0xFF00) >> 8;	//Transfer length MSB
	packet->comData[8] = (sectorCount & 0xFF);			//Transfer length LSB
	packet->comData[9] = 0;			//control
}

inline void cbw_scsi_write_sector(cbw_packet* packet, int lba, int sectorSize, int sectorCount)
{
	packet->tag = -TAG_WRITE;
	packet->dataTransferLength = sectorSize	 * sectorCount;
	packet->flags = 0x00;			//write data will flow Out
	packet->comLength = 10;			//scsi command of size 10

	//scsi command packet
	packet->comData[0] = 0x2A;		//WRITE(10) operation code
	packet->comData[1] = 0;			//LUN/DPO/FUA/Reserved/Reldr
	packet->comData[2] = (lba & 0xFF000000) >> 24;	//lba 1 (MSB)
	packet->comData[3] = (lba & 0xFF0000) >> 16;	//lba 2
	packet->comData[4] = (lba & 0xFF00) >> 8;	//lba 3
	packet->comData[5] = (lba & 0xFF);		//lba 4 (LSB)
	packet->comData[6] = 0;			//Reserved
	packet->comData[7] = (sectorCount & 0xFF00) >> 8;	//Transfer length MSB
	packet->comData[8] = (sectorCount & 0xFF);			//Transfer length LSB
	packet->comData[9] = 0;			//control
}

void usb_callback(int resultCode, int bytes, void *arg)
{
	usb_callback_data* data = (usb_callback_data*) arg;
	data->returnCode = resultCode;
	data->returnSize = bytes;
	XPRINTF("Usb - callback: res %d, bytes %d, arg %p \n", resultCode, bytes, arg);
	SignalSema(data->semh);
}

void set_configuration(mass_dev* dev, int configNumber) {
	int ret;
	usb_callback_data cb_data;
	iop_sema_t s;

	s.initial = 0;
	s.max = 1;
	s.option = 0;
	s.attr = 0;
	cb_data.semh = CreateSema(&s);

	XPRINTF("setting configuration controlEp=%i, confNum=%i \n", dev->controlEp, configNumber);

	ret = UsbSetDeviceConfiguration(dev->controlEp, configNumber, usb_callback, (void*)&cb_data);

	if(ret != USB_RC_OK)
	{
		XPRINTF("Usb: Error sending set_configuration\n");
	}
	else
	{
		XPRINTF("Waiting for config done...\n");
		WaitSema(cb_data.semh);
	}

	DeleteSema(cb_data.semh);
}

void set_interface(mass_dev* dev, int interface, int altSetting) {
	int ret;
	usb_callback_data cb_data;
	iop_sema_t s;

	s.initial = 0;
	s.max = 1;
	s.option = 0;
	s.attr = 0;
	cb_data.semh = CreateSema(&s);


	XPRINTF("setting interface controlEp=%i, interface=%i altSetting=%i\n", dev->controlEp, interface, altSetting);

	ret = UsbSetInterface(dev->controlEp, interface, altSetting, usb_callback, (void*)&cb_data);

        if(ret != USB_RC_OK) {
		XPRINTF("Usb: Error sending set_interface\n");
	} else {
		XPRINTF("Waiting for interface done...\n");
		WaitSema(cb_data.semh);
	}
	DeleteSema(cb_data.semh);

}

void set_device_feature(mass_dev* dev, int feature) {
	int ret;
	usb_callback_data cb_data;
	iop_sema_t s;

	s.initial = 0;
	s.max = 1;
	s.option = 0;
	s.attr = 0;
	cb_data.semh = CreateSema(&s);

	XPRINTF("setting device feature controlEp=%i, feature=%i\n", dev->controlEp, feature);

	ret = UsbSetDeviceFeature(dev->controlEp, feature, usb_callback, (void*)&cb_data);

	if(ret != USB_RC_OK) {
		XPRINTF("Usb: Error sending set_device feature\n");
	} else {
		XPRINTF("Waiting for set device feature done...\n");
		WaitSema(cb_data.semh);
	}
	DeleteSema(cb_data.semh);

}

void usb_bulk_clear_halt(mass_dev* dev, int direction) {
	int ret;
	usb_callback_data cb_data;
	iop_sema_t s;
	int endpoint;

	s.initial = 0;
	s.max = 1;
	s.option = 0;
	s.attr = 0;

	if (direction == 0) {
		endpoint = dev->bulkEpIAddr;
		//endpoint = dev->bulkEpI;
	} else {
		endpoint = dev->bulkEpOAddr;
	}

	cb_data.semh = CreateSema(&s);
	ret = UsbClearEndpointFeature(
		dev->controlEp, 		//Config pipe
		0,			//HALT feature
		endpoint,
		usb_callback,
		(void*)&cb_data
		);

	if(ret != USB_RC_OK) {
		XPRINTF("Usb: Error sending clear halt\n");
		return;
	}else {
		XPRINTF("wait clear halt semaId=%i\n", cb_data.semh);
		WaitSema(cb_data.semh);
	}

	DeleteSema(cb_data.semh);

}

void usb_bulk_reset(mass_dev* dev, int mode) {
	int ret;
	usb_callback_data cb_data;
	iop_sema_t s;

	s.initial = 0;
	s.max = 1;
	s.option = 0;
	s.attr = 0;
	cb_data.semh = CreateSema(&s);

	//Call Bulk only mass storage reset
	ret = UsbControlTransfer(
		dev->controlEp, 		//default pipe
		0x21,			//bulk reset
		0xFF,
		0,
		dev->interfaceNumber, //interface number
		0,			//length
		NULL,			//data
		usb_callback,
		(void*) &cb_data
		);

	if(ret != USB_RC_OK) {
		XPRINTF("Usb: Error sending reset request\n");
		return;
	}else {
		XPRINTF("wait reset semaId=%i\n", cb_data.semh);
		WaitSema(cb_data.semh);
	}
	DeleteSema(cb_data.semh);

	//clear bulk-in endpoint
	if (mode & 0x01) {
		usb_bulk_clear_halt(dev, 0);
	}

	//clear bulk-out endpoint
	if (mode & 0x02) {
		usb_bulk_clear_halt(dev, 1);
	}
}

int usb_bulk_status(mass_dev* dev, csw_packet* csw, int tag) {
	int ret;
	iop_sema_t s;
	usb_callback_data cb_data;

	s.initial = 0;
	s.max = 1;
	s.option = 0;
	s.attr = 0;
	cb_data.semh = CreateSema(&s);

	initCSWPacket(csw);
	csw->tag = tag;
	csw->dataResidue = residue;
	csw->status = 0;

	ret =  UsbBulkTransfer(
		dev->bulkEpI,		//bulk input pipe
		csw,			//data ptr
		13,			//data length
		usb_callback,
		(void*)&cb_data
	);
	if(ret != USB_RC_OK) {
		XPRINTF("Usb: Error sending csw bulk command\n");
		DeleteSema(cb_data.semh);
		return -1;
	}else {
		WaitSema(cb_data.semh);
	}
	DeleteSema(cb_data.semh);
	XPRINTF("Usb: bulk csw.status: %i\n", csw->status);
	return csw->status;
}

/* see flow chart in the usbmassbulk_10.pdf doc (page 15) */
int usb_bulk_manage_status(mass_dev* dev, int tag) {
	int ret;
	csw_packet csw;

	XPRINTF("...waiting for status 1 ...\n");
	ret = usb_bulk_status(dev, &csw, tag); /* Attempt to read CSW from bulk in endpoint */
    if (ret == USB_RC_STALL || ret < 0) { /* STALL bulk in  -OR- Bulk error */
		usb_bulk_clear_halt(dev, 0); /* clear the stall condition for bulk in */

		XPRINTF("...waiting for status 2 ...\n");
		ret = usb_bulk_status(dev, &csw, tag); /* Attempt to read CSW from bulk in endpoint */
	}

	/* CSW not valid  or stalled or phase error */
	if (csw.signature !=  0x53425355 || csw.tag != tag) {
		XPRINTF("...call reset recovery ...\n");
		usb_bulk_reset(dev, 3);	/* Perform reset recovery */
	}

   if (ret != 0) {
      XPRINTF("Mass storage device not ready! %d\n", ret);
      return 1; //device is not ready, need to retry!
   } else {
      return 0; //device is ready to process next CBW
   }
}

void usb_bulk_command(mass_dev* dev, cbw_packet* packet ) {
	int ret;
	iop_sema_t s;
	usb_callback_data cb_data;

	s.initial = 0;
	s.max = 1;
	s.option = 0;
	s.attr = 0;
	cb_data.semh = CreateSema(&s);

	ret =  UsbBulkTransfer(
		dev->bulkEpO,		//bulk output pipe
		packet,			//data ptr
		31,			//data length
		usb_callback,
		(void*)&cb_data
	);
	if(ret != USB_RC_OK) {
		XPRINTF("Usb: Error sending bulk command\n");
	}else {
		WaitSema(cb_data.semh);
	}
	DeleteSema(cb_data.semh);

}

int usb_bulk_transfer(int pipe, void* buffer, int transferSize) {
	int ret;
	char* buf = (char*) buffer;
	int blockSize = transferSize;
	int offset = 0;

	iop_sema_t s;
	usb_callback_data cb_data;

	s.initial = 0;
	s.max = 1;
	s.option = 0;
	s.attr = 0;
	cb_data.semh = CreateSema(&s);

	while (transferSize > 0) {
		if (transferSize < blockSize) {
			blockSize = transferSize;
		}

		ret =  UsbBulkTransfer(
			pipe,		//bulk pipe epI(Read)  epO(Write)
			(buf + offset),		//data ptr
			blockSize,		//data length
			usb_callback,
			(void*)&cb_data
		);
		if(ret != USB_RC_OK) {
			XPRINTF("Usb: Error sending bulk data transfer \n");
			cb_data.returnCode = -1;
			break;
		}else {
			WaitSema(cb_data.semh);
			XPRINTF(" retCode=%i retSize=%i \n", cb_data.returnCode, cb_data.returnSize);
			if (cb_data.returnCode > 0) {
				residue = blockSize;
				break;
			}
			offset += cb_data.returnSize;
			transferSize-= cb_data.returnSize;
		}
	}
	DeleteSema(cb_data.semh);
	return cb_data.returnCode;
}

mass_dev* mass_stor_findDevice(int devId, int create)
{
    mass_dev* dev = NULL;
    int i;
    XPRINTF("usb_mass: mass_stor_findDevice devId %i\n", devId);
    for (i = 0; i < NUM_DEVICES; ++i)
    {
        if (g_mass_device[i].devId == devId)
        {
            XPRINTF("usb_mass: mass_stor_findDevice exists %i\n", i);
            return &g_mass_device[i];
        }
        else if (create && dev == NULL && g_mass_device[i].devId == -1)
        {
            dev = &g_mass_device[i];
        }
    }
    return dev;
}

/* size should be a multiple of sector size */
int mass_stor_readSector(mass_dev* mass_device, unsigned int sector, unsigned char* buffer, int size) {
	cbw_packet cbw;
	int stat;

	/* assume device is detected and configured - should be checked in upper levels */

	initCBWPacket(&cbw);

	cbw_scsi_read_sector(&cbw, sector, mass_device->sectorSize, size/mass_device->sectorSize);  // Added by Hermes

	stat = 1;
	while (stat != 0) {
		XPRINTF("-READ SECTOR COMMAND: %i\n", sector);
		usb_bulk_command(mass_device, &cbw);

		XPRINTF("-READ SECTOR DATA\n");
		stat = usb_bulk_transfer(mass_device->bulkEpI, buffer, size); //Modified by Hermes

       	XPRINTF("-READ SECTOR STATUS\n");
		stat = usb_bulk_manage_status(mass_device, -TAG_READ);
	}
	return size; //Modified by Hermes
}

/* size should be a multiple of sector size */
int mass_stor_writeSector(mass_dev* mass_device, unsigned int sector, unsigned char* buffer, int size) {
	cbw_packet cbw;
	int stat;

	/* assume device is detected and configured - should be checked in upper levels */

	initCBWPacket(&cbw);

	cbw_scsi_write_sector(&cbw, sector, mass_device->sectorSize, size/mass_device->sectorSize);

	stat = 1;
	while (stat != 0) {
		XPRINTF("-WRITE SECTOR COMMAND: %i\n", sector);
		usb_bulk_command(mass_device, &cbw);

		XPRINTF("-WRITE SECTOR DATA\n");
		stat = usb_bulk_transfer(mass_device->bulkEpO, buffer, size);

       	XPRINTF("-READ SECTOR STATUS\n");
		stat = usb_bulk_manage_status(mass_device, -TAG_WRITE);
	}
	return size;
}


/* test that endpoint is bulk endpoint and if so, update device info */
void usb_bulk_probeEndpoint(int devId, mass_dev* dev, UsbEndpointDescriptor* endpoint) {
	if (endpoint->bmAttributes == USB_ENDPOINT_XFER_BULK) {
		/* out transfer */
		if ((endpoint->bEndpointAddress & 0x80) == 0 && dev->bulkEpO < 0) {
			dev->bulkEpOAddr = endpoint->bEndpointAddress;
			dev->bulkEpO = UsbOpenEndpointAligned(devId, endpoint);
			dev->packetSzO = endpoint->wMaxPacketSizeHB * 256 + endpoint->wMaxPacketSizeLB;
			XPRINTF("register Output endpoint id =%i addr=%02X packetSize=%i\n", dev->bulkEpO,dev->bulkEpOAddr, dev->packetSzO);
		}else
		/* in transfer */
		if ((endpoint->bEndpointAddress & 0x80) != 0 && dev->bulkEpI < 0) {
			dev->bulkEpIAddr = endpoint->bEndpointAddress;
			dev->bulkEpI = UsbOpenEndpointAligned(devId, endpoint);
			dev->packetSzI = endpoint->wMaxPacketSizeHB * 256 + endpoint->wMaxPacketSizeLB;
			XPRINTF("register Intput endpoint id =%i addr=%02X packetSize=%i\n", dev->bulkEpI, dev->bulkEpIAddr, dev->packetSzI);
		}
	}
}


int mass_stor_probe(int devId)
{
	UsbDeviceDescriptor *device = NULL;
	UsbConfigDescriptor *config = NULL;
	UsbInterfaceDescriptor *intf = NULL;

	XPRINTF("usb_mass: probe: devId=%i\n", devId);

    // TODO Dont always create
    mass_dev* mass_device = mass_stor_findDevice(devId, 0);
    
	/* only one device supported */
	if((mass_device != NULL) && (mass_device->status & DEVICE_DETECTED))
	{
		XPRINTF("mass_stor: Error: only one mass storage device allowed ! \n");
		return 0;
	}

	/* get device descriptor */
	device = (UsbDeviceDescriptor*)UsbGetDeviceStaticDescriptor(devId, NULL, USB_DT_DEVICE);
	if (device == NULL)  {
		printf("mass_stor: Error: Couldn't get device descriptor\n");
		return 0;
	}

	/* Check if the device has at least one configuration */
	if(device->bNumConfigurations < 1) { return 0; }

	/* read configuration */
	config = (UsbConfigDescriptor*)UsbGetDeviceStaticDescriptor(devId, device, USB_DT_CONFIG);
	if (config == NULL) {
	      printf("mass_stor: Error: Couldn't get configuration descriptor\n");
	      return 0;
	}
	/* check that at least one interface exists */
	if(	(config->bNumInterfaces < 1) ||
		(config->wTotalLength < (sizeof(UsbConfigDescriptor) + sizeof(UsbInterfaceDescriptor)))) {
		      printf("mass_stor: Error: No interfaces available\n");
		      return 0;
	}
        /* get interface */
	intf = (UsbInterfaceDescriptor *) ((char *) config + config->bLength); /* Get first interface */

	if(	(intf->bInterfaceClass		!= USB_CLASS_MASS_STORAGE) ||
		(intf->bInterfaceSubClass	!= USB_SUBCLASS_MASS_SCSI  &&
		 intf->bInterfaceSubClass	!= USB_SUBCLASS_MASS_SFF_8070I) ||
		(intf->bInterfaceProtocol	!= USB_PROTOCOL_MASS_BULK_ONLY) ||
		(intf->bNumEndpoints < 2))    { //one bulk endpoint is not enough because
			 return 0;		//we send the CBW to te bulk out endpoint
	}
	return 1;
}

int mass_stor_connect(int devId)
{
	int i;
	int epCount;
	UsbDeviceDescriptor *device;
	UsbConfigDescriptor *config;
	UsbInterfaceDescriptor *interface;
	UsbEndpointDescriptor *endpoint;
	mass_dev* dev;

	wait_for_connect = 0;

	printf ("usb_mass: connect: devId=%i\n", devId);
	dev = mass_stor_findDevice(devId, 1);

	if (dev == NULL) {
		printf("usb_mass: Error - unable to allocate space!\n");
		return 1;
	}

	/* only one mass device allowed */
	if (dev->devId != -1) {
		printf("usb_mass: Error - only one mass storage device allowed !\n");
		return 1;
	}

	dev->status = 0;
	dev->sectorSize = 0;

	dev->bulkEpI = -1;
	dev->bulkEpO = -1;

	/* open the config endpoint */
	dev->controlEp = UsbOpenEndpoint(devId, NULL);

	device = (UsbDeviceDescriptor*)UsbGetDeviceStaticDescriptor(devId, NULL, USB_DT_DEVICE);

	config = (UsbConfigDescriptor*)UsbGetDeviceStaticDescriptor(devId, device, USB_DT_CONFIG);

	interface = (UsbInterfaceDescriptor *) ((char *) config + config->bLength); /* Get first interface */
	// store interface numbers
	dev->interfaceNumber = interface->bInterfaceNumber;
	dev->interfaceAlt    = interface->bAlternateSetting;

	epCount = interface->bNumEndpoints;
	endpoint = (UsbEndpointDescriptor*) UsbGetDeviceStaticDescriptor(devId, NULL, USB_DT_ENDPOINT);
	usb_bulk_probeEndpoint(devId, dev, endpoint);

	for (i = 1; i < epCount; i++)
	{
		endpoint = (UsbEndpointDescriptor*) ((char *) endpoint + endpoint->bLength);
		usb_bulk_probeEndpoint(devId, dev, endpoint);
	}

	/* we do NOT have enough bulk endpoints */
	if (dev->bulkEpI < 0  /* || dev->bulkEpO < 0 */ ) { /* the bulkOut is not needed now */
		if (dev->bulkEpI >= 0) {
			UsbCloseEndpoint(dev->bulkEpI);
		}
		if (dev->bulkEpO >= 0) {
			UsbCloseEndpoint(dev->bulkEpO);
		}
		XPRINTF(" connect failed: not enough bulk endpoints! \n");
		return -1;
	}

	/*store current configuration id - can't call set_configuration here */
	dev->devId = devId;
	dev->configId = config->bConfigurationValue;
	dev->status = DEVICE_DETECTED;
	XPRINTF("usb_mass: connect ok: epI=%i, epO=%i \n", dev->bulkEpI, dev->bulkEpO);

	return 0;
}

void mass_stor_release(mass_dev *dev)
{
	if (dev->bulkEpI >= 0)
	{
		UsbCloseEndpoint(dev->bulkEpI);
	}

	if (dev->bulkEpO >= 0)
	{
		UsbCloseEndpoint(dev->bulkEpO);
	}

	dev->bulkEpI = -1;
	dev->bulkEpO = -1;
	dev->controlEp = -1;
	dev->status = 0;
}

int mass_stor_disconnect(int devId) {
	mass_dev* dev;
	dev = mass_stor_findDevice(devId, 0);

	printf ("usb_mass: disconnect: devId=%i\n", devId);

    if (dev == NULL)
    {
        printf ("usb_mass: disconnect: no device storage!\n");
        return 0;
    }
    
	if ((dev->status & DEVICE_DETECTED) && devId == dev->devId)
	{
		mass_stor_release(dev);
		fat_disconnect(dev);
		scache_kill(dev->cache);
		dev->cache = NULL;
		dev->devId = -1;
	}
	return 0;
}


int mass_stor_warmup(mass_dev *dev) {
	cbw_packet cbw;
	unsigned char buffer[64];
	int stat;
	int retryCount;
    int ready;
    sense_data *sd;

	XPRINTF("mass_stor: warm up called ! \n");
	if (!(dev->status & DEVICE_DETECTED)) {
		printf("usb_mass: Error - no mass storage device found!\n");
		return -ENODEV;
	}

	initCBWPacket(&cbw);

	//send inquiry command
	memset(buffer, 0, 64);
	cbw_scsi_inquiry(&cbw);
	XPRINTF("-INQUIRY\n");
	usb_bulk_command(dev, &cbw);

	XPRINTF("-INQUIRY READ DATA\n");
	int returnCode = usb_bulk_transfer(dev->bulkEpI, buffer, 36);

	XPRINTF("-INQUIRY STATUS\n");
	residue = 0;
	usb_bulk_manage_status(dev, -TAG_INQUIRY);

	if (returnCode < 0) {
		printf("!Error: device not ready!\n");
		return -ENODEV;
	}

    ready = 0;
    while(!ready)
    {
    	//send start stop command
    	cbw_scsi_start_stop_unit(&cbw);
    	XPRINTF("-START COMMAND\n");
    	usb_bulk_command(dev, &cbw);

    	XPRINTF("-START COMMAND STATUS\n");
    	usb_bulk_manage_status(dev, -TAG_START_STOP_UNIT);

        //send test unit ready command
        cbw_scsi_test_unit_ready(&cbw);
        XPRINTF("-TUR COMMAND\n");
        usb_bulk_command(dev, &cbw);
        XPRINTF("-TUR STATUS\n");

        if(usb_bulk_manage_status(dev, -TAG_TEST_UNIT_READY) == 0)
        {
            ready = 1;
        }
        else
        {
        	//send "request sense" command
        	cbw_scsi_request_sense(&cbw); //prepare scsi command block

        	XPRINTF("-REQUEST SENSE COMMAND\n");
        	usb_bulk_command(dev, &cbw);

        	XPRINTF("-RS DATA\n");
        	usb_bulk_transfer(dev->bulkEpI, buffer, 18);

        	XPRINTF("-RS STATUS\n");
        	stat = usb_bulk_manage_status(dev, -TAG_REQUEST_SENSE);

        	sd = (sense_data *) buffer;

        	// TODO: Some devices may require additional initialization and report that via the sense key.
        	// If this happens, that init should be done here and we should not return but rather send
        	// the START command again and retry the TEST UNIT READY command.
        	// see usbmass-ufi10.pdf

            if((sd->sense_key == 0x02) && (sd->add_sense_code == 0x04) && (sd->add_sense_qual == 0x02))
            {
                printf("USB Error: Additional initalization is required for this device!\n");
//                return(-1);
            }

        	XPRINTF("Sense Data:\n");
        	XPRINTF("error code: %02X\n", sd->error_code);
        	XPRINTF("sense key:  %02X\n", sd->sense_key);

        	XPRINTF("information: %02X, %02X, %02X, %02X\n", sd->information[0], sd->information[1], sd->information[2], sd->information[3]);

        	XPRINTF("add sense len:  %02X\n", sd->add_sense_len);

        	XPRINTF("add sense code:  %02X\n", sd->add_sense_code);
        	XPRINTF("add sense qual:  %02X\n", sd->add_sense_qual);

//        	return(-1);
        }
    }


	//send "request sense" command
	//required for correct operation of some devices
	//discovered and reported by BraveDog
	cbw_scsi_request_sense(&cbw); //prepare scsi command block

	XPRINTF("-REQUEST SENSE COMMAND\n");
	usb_bulk_command(dev, &cbw);

	XPRINTF("-RS DATA\n");
	usb_bulk_transfer(dev->bulkEpI, buffer, 18);

	XPRINTF("-RS STATUS\n");
	stat = usb_bulk_manage_status(dev, -TAG_REQUEST_SENSE);

	//send read capacity command
	cbw_scsi_read_capacity(&cbw); //prepare scsi command block

	stat = 1;
	retryCount = 6;
	while (stat != 0 && retryCount > 0) {
		XPRINTF("-READ CAPACITY COMMAND\n");
		usb_bulk_command(dev, &cbw);

		XPRINTF("-RC DATA\n");
		int returnCode = usb_bulk_transfer(dev->bulkEpI, buffer, 8);

		//HACK HACK HACK !!!
		//according to usb doc we should allways
		//attempt to read the CSW packet. But in some cases
		//reading of CSW packet just freeze ps2....:-(
        if (returnCode == USB_RC_STALL) {
            XPRINTF("call reset recovery ...\n");
			usb_bulk_reset(dev, 1);
		} else {

			XPRINTF("-RC STATUS\n");
			stat = usb_bulk_manage_status(dev, -TAG_READ_CAPACITY);
		}
		retryCount --;
	}
	if (stat != 0) {
		printf("mass_stor: Warning: warmup (read capacity) failed! \n");
		return 1;
	}

	dev->sectorSize = getBI32 ( &buffer[ 4 ] );
	dev->maxLBA    = getBI32 ( &buffer[ 0 ] );

	return 0;

}

void mass_stor_configureDevices()
{
	int i;

    //dlanor: this form of delay is inappropriate as mass_stor_configureDevices
    //dlanor: is called by nearly all driver functions. So for all we know
    //dlanor: the USBD driver may have had hours in which to detect a device
    //dlanor: Having the delay here merely prevents applications from doing
    //dlanor: any other useful work while waiting for USB_mass connections
		//dlanor: So I'm now disabling this delay (by commenting)
    // give the USB driver some time to detect the device
    //i = 10000;
    //while (wait_for_connect && (--i > 0))
    //    DelayThread(100);

	XPRINTF("mass_stor: configuring devices... \n");

    for (i = 0; i < NUM_DEVICES; ++i)
    {
        mass_dev *dev = &g_mass_device[i];
        if (dev->devId != -1 && (dev->status & DEVICE_DETECTED) && !(dev->status & DEVICE_CONFIGURED))
        {
            int ret;
            set_configuration(dev, dev->configId);
            set_interface(dev, dev->interfaceNumber, dev->interfaceAlt);
            dev->status |= DEVICE_CONFIGURED;

            ret = mass_stor_warmup(dev);
            if (ret < 0)
            {
                printf("mass_stor: failed to warmup device %d\n", dev->devId);
                mass_stor_release(dev);
                break;
            }

            dev->cache = scache_init(dev, dev->sectorSize); // modified by Hermes
            if (dev->cache == NULL) {
                printf ("mass_stor: scache_init failed \n" );
                break;
            }

            fat_connect(dev);
        }
    }
}

int InitUSB()
{
    int i;
	int ret = 0;
    for (i = 0; i < NUM_DEVICES; ++i)
        g_mass_device[i].devId = -1;

	driver.next 		= NULL;
	driver.prev		    = NULL;
	driver.name 		= "mass-stor";
	driver.probe 		= mass_stor_probe;
	driver.connect		= mass_stor_connect;
	driver.disconnect	= mass_stor_disconnect;

	ret = UsbRegisterDriver(&driver);
	XPRINTF("mass_stor: registerDriver=%i \n", ret);
	if (ret < 0)
	{
		printf("usb_mass: register driver failed! ret=%d\n", ret);
		return(-1);
	}

    return(0);
}
