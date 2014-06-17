/*
 * mass_stor.c - USB Mass storage driver for PS2
 *
 * (C) 2002, David Ryan ( oobles@hotmail.com )
 * (C) 2003, TyRaNiD <tiraniddo@hotmail.com>
 * (C) 2004, Marek Olejnik (ole00@post.cz)
 * (C) 2004  Hermes (support for sector sizes from 512 to 4096 bytes)
 *
 * Other contributors and testers: Bigboss, Sincro, Spooo, BraveDog.
 * 
 * This module handles the setup and manipulation of USB mass storage devices
 * on the PS2
 *
 * See the file LICENSE included with this distribution for licensing terms.
 */

#include <tamtypes.h>
#include <sifrpc.h>
#include <thsemap.h>
#include <usbd.h>
#include <usbd_macro.h>
#include "mass_stor.h"
//#define DEBUG 1
#include "mass_debug.h"

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
#define DEVICE_DISCONNECTED 4

#define MASS_CONNECT_CALLBACK    0x0012
#define MASS_DISCONNECT_CALLBACK 0x0013

// Added by Hermes 
int getBI32(unsigned char* buf) {
	return (buf[3]  + (buf[2] <<8) + (buf[1] << 16) + (buf[0] << 24));
}
unsigned Size_Sector=512; // store size of sector from usb mass
unsigned g_MaxLBA;


typedef struct _mass_dev
{
	int controlEp;		//config endpoint id
	int bulkEpI;		//in endpoint id
	unsigned char bulkEpIAddr; // in endpoint address
	int bulkEpO;		//out endpoint id
	unsigned char bulkEpOAddr; // out endpoint address
	int packetSzI;		//packet size in
	int packetSzO;		//packet size out
	int devId;		//device id
	int configId;	//configuration id
	int status;
	int interfaceNumber;	//interface number
	int interfaceAlt;	//interface alternate setting
} mass_dev;

typedef struct _read_info {
	mass_dev* dev;
	void* buffer;
	int transferSize;
	int offset;
	int returnCode;
	int num;
	int semh;
} read_info;

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

//void usb_bulk_read1(int resultCode, int bytes, void* arg);
void usb_bulk_read2(int resultCode, int bytes, void* arg);

UsbDriver driver;

int retCode = 0;
int retSize = 0;


static int returnCode;
static int returnSize;
static int residue;

mass_dev mass_device;		//current device 

void initCBWPacket(cbw_packet* packet) {
	packet->signature = 0x43425355; 
	packet->flags = 0x80; //80->data in,  00->out 
	packet->lun = 0;
}

void initCSWPacket(csw_packet* packet) {
	packet->signature = 0x53425355; 
}
void cbw_scsi_test_unit_ready(cbw_packet* packet) {
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

void cbw_scsi_request_sense(cbw_packet* packet) {
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

void cbw_scsi_inquiry(cbw_packet* packet) {
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

void cbw_scsi_start_stop_unit(cbw_packet* packet) {
	packet->tag = - TAG_START_STOP_UNIT;
	packet->dataTransferLength = 0;	//START_STOP_REPLY_LENGTH
	packet->flags = 0x80;			//inquiry data will flow In
	packet->comLength = 6;			//short SCSI command

	//scsi command packet
	packet->comData[0] = 0x1B;		//start stop unit operation code
	packet->comData[1] = 1;			//lun/reserved/immed
	packet->comData[2] = 0;			//reserved
	packet->comData[3] = 0;			//reserved
	packet->comData[4] = 3;			//reserved/LoEj/Start (load and stard)
	packet->comData[5] = 0;			//control
}

void cbw_scsi_read_capacity(cbw_packet* packet) {
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

void cbw_scsi_read_sector(cbw_packet* packet, int lba, int sectorSize, int sectorCount) {
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

void cbw_scsi_write_sector(cbw_packet* packet, int lba, int sectorSize, int sectorCount) {
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

void usb_callback(int resultCode, int bytes, void *arg) {
	int semh = (int) arg;
	returnCode = resultCode;
	returnSize = bytes;
	XPRINTF("Usb - callback: res %d, bytes %d, arg %p \n", resultCode, bytes, arg);
	SignalSema(semh);
}

void set_configuration(mass_dev* dev, int configNumber) {
	int ret;
	int semh;
	iop_sema_t s;

	s.initial = 0;
	s.max = 1;
	s.option = 0;
	s.attr = 0;
	semh = CreateSema(&s);


	XPRINTF("setting configuration controlEp=%i, confNum=%i \n", dev->controlEp, configNumber);

	ret = UsbSetDeviceConfiguration(dev->controlEp, configNumber, usb_callback, (void*)semh);

	if(ret != USB_RC_OK) {
		XPRINTF("Usb: Error sending set_configuration\n");
	} else {
		XPRINTF("Waiting for config done...\n");
		WaitSema(semh);
	}
	DeleteSema(semh);

}

void set_interface(mass_dev* dev, int interface, int altSetting) {
	int ret;
	int semh;
	iop_sema_t s;

	s.initial = 0;
	s.max = 1;
	s.option = 0;
	s.attr = 0;
	semh = CreateSema(&s);


	XPRINTF("setting interface controlEp=%i, interface=%i altSetting=%i\n", dev->controlEp, interface, altSetting);

	ret = UsbSetInterface(dev->controlEp, interface, altSetting, usb_callback, (void*)semh);

        if(ret != USB_RC_OK) {
		XPRINTF("Usb: Error sending set_interface\n");
	} else {
		XPRINTF("Waiting for interface done...\n");
		WaitSema(semh);
	}
	DeleteSema(semh);

}

void set_device_feature(mass_dev* dev, int feature) {
	int ret;
	int semh;
	iop_sema_t s;

	s.initial = 0;
	s.max = 1;
	s.option = 0;
	s.attr = 0;
	semh = CreateSema(&s);


	XPRINTF("setting device feature controlEp=%i, feature=%i altSetting=%i\n", dev->controlEp, feature);

	ret = UsbSetDeviceFeature(dev->controlEp, feature, usb_callback, (void*)semh);

	if(ret != USB_RC_OK) {
		XPRINTF("Usb: Error sending set_device feature\n");
	} else {
		XPRINTF("Waiting for set device feature done...\n");
		WaitSema(semh);
	}
	DeleteSema(semh);

}

void usb_bulk_clear_halt(mass_dev* dev, int direction) {
	int ret;
	int semh;
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

	semh = CreateSema(&s);
	ret = UsbClearEndpointFeature(
		dev->controlEp, 		//Config pipe
		0,			//HALT feature
		endpoint,
		usb_callback, 
		(void*)semh
		);

	if(ret != USB_RC_OK) {
		XPRINTF("Usb: Error sending clear halt\n");
		return;
	}else {
		XPRINTF("wait clear halt semaId=%i\n", semh);
		WaitSema(semh);
	}

	DeleteSema(semh);

}

void usb_bulk_reset(mass_dev* dev, int mode) {
	int ret;
	int semh;
	iop_sema_t s;

	s.initial = 0;
	s.max = 1;
	s.option = 0;
	s.attr = 0;
	semh = CreateSema(&s);

	//Call Bulk only mass storage reset
	ret = UsbControlTransfer(
		dev->controlEp, 		//default pipe
		0x21,			//bulk reset
		0xFF,
		0,
		0,			//interface number  FIXME - correct number
		0,			//length
		NULL,			//data
		usb_callback, 
		(void*) semh
		);

	if(ret != USB_RC_OK) {
		XPRINTF("Usb: Error sending reset request\n");
		return;
	}else {
		XPRINTF("wait reset semaId=%i\n", semh);
		WaitSema(semh);
	}
	DeleteSema(semh);

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
	int semh;

	s.initial = 0;
	s.max = 1;
	s.option = 0;
	s.attr = 0;
	semh = CreateSema(&s);

	initCSWPacket(csw);
	csw->tag = tag;
	csw->dataResidue = residue;
	csw->status = 0;

	ret =  UsbBulkTransfer(
		dev->bulkEpI,		//bulk input pipe
		csw,			//data ptr
		13,			//data length
		usb_callback,
		(void*)semh
	);
	if(ret != USB_RC_OK) {
		XPRINTF("Usb: Error sending csw bulk command\n");
		DeleteSema(semh);
		return -1;
	}else {
		WaitSema(semh);
	}
	DeleteSema(semh);
	XPRINTF("Usb: bulk csw.status: %i\n", csw->status);
	return csw->status;
}

/* see flow chart in the usbmassbulk_10.pdf doc (page 15) */
int usb_bulk_manage_status(mass_dev* dev, int tag) {
	int ret;
	csw_packet csw;

	XPRINTF("...waiting for status 1 ...\n");
	ret = usb_bulk_status(dev, &csw, tag); /* Attempt to read CSW from bulk in endpoint */
	if (ret == 4 || ret < 0) { /* STALL bulk in  -OR- Bulk error */
		usb_bulk_clear_halt(dev, 0); /* clear the stall condition for bulk in */
		
		XPRINTF("...waiting for status 2 ...\n");
		ret = usb_bulk_status(dev, &csw, tag); /* Attempt to read CSW from bulk in endpoint */
		
	}

	/* CSW not valid  or stalled or phase error */
	if (csw.signature !=  0x53425355 || csw.tag != tag || ret != 0) {
		XPRINTF("...call reset recovery ...\n");
		usb_bulk_reset(dev, 3);	/* Perform reset recovery */
	}

	return 0; //device is ready to process next CBW
}


void usb_bulk_command(mass_dev* dev, cbw_packet* packet ) {
	int ret;
	iop_sema_t s;
	int semh;

	s.initial = 0;
	s.max = 1;
	s.option = 0;
	s.attr = 0;
	semh = CreateSema(&s);

	ret =  UsbBulkTransfer(
		dev->bulkEpO,		//bulk output pipe
		packet,			//data ptr
		31,			//data length
		usb_callback,
		(void*)semh
	);
	if(ret != USB_RC_OK) {
		XPRINTF("Usb: Error sending bulk command\n");
	}else {
		WaitSema(semh);
	}
	DeleteSema(semh);

}


int usb_bulk_transfer(int pipe, void* buffer, int transferSize) {
	int ret;
	char* buf = (char*) buffer;
	int blockSize = transferSize;
	int offset = 0;
	int ep; //endpoint

	iop_sema_t s;
	int semh;

	s.initial = 0;
	s.max = 1;
	s.option = 0;
	s.attr = 0;
	semh = CreateSema(&s);

	while (transferSize > 0) {
		if (transferSize < blockSize) {
			blockSize = transferSize;
		}	

		ret =  UsbBulkTransfer(
			pipe,		//bulk pipe epI(Read)  epO(Write)
			(buf + offset),		//data ptr
			blockSize,		//data length
			usb_callback,
			(void*)semh
		);
		if(ret != USB_RC_OK) {
			XPRINTF("Usb: Error sending bulk data transfer \n");
			returnCode = -1;
			break;
		}else {
			WaitSema(semh);
			XPRINTF(" retCode=%i retSize=%i \n", returnCode, returnSize);
			if (returnCode > 0) {
				residue = blockSize;
				break;
			}
			offset += returnSize;
			transferSize-= returnSize;
		}
	}
	DeleteSema(semh);
	return returnCode;
}


/* helper functions */
#ifdef COMPILE_DUMPS
void dumpDevice(UsbDeviceDescriptor* data) {
	printf("\n");
	printf("DEVICE info\n");
	printf("-----------\n");
	printf("bcdUSB=0x%04X\n", data->bcdUSB);
	printf("class=0x%04X\n", data->bDeviceClass);
	printf("subclass=0x%04X \n" , data->bDeviceSubClass);
	printf("vendor=0x%04X \n", data->idVendor);
	printf("product=0x%04X \n", data->idProduct);
	printf("num configurations=0x%04X \n", data->bNumConfigurations);
	printf("index manufacturer=%i \n", data->iManufacturer);
	printf("index product=%i \n", data->iProduct);
}

void dumpConfig(UsbConfigDescriptor* data) {
	printf("\n");
	printf("CONFIG info\n");
	printf("-----------\n");
	printf("num interfaces=0x%02X\n", data->bNumInterfaces);
	printf("config value=0x%02X\n", data->bConfigurationValue);
	printf("index configuration=0x%02X\n", data->iConfiguration);
	printf("attributes=0x%02X\n", data->bmAttributes);
	printf("max power=0x%02X\n", data->MaxPower);

}

void dumpInterface(UsbInterfaceDescriptor* data) {
	printf("\n");
	printf("INTERFACE info\n");
	printf("--------------\n");
	printf("interface number=0x%02X\n", data->bInterfaceNumber);
	printf("alternate setting=0x%02X\n", data->bAlternateSetting);
	printf("num endpoints=0x%02X\n", data->bNumEndpoints);
	printf("class=0x%02X\n", data->bInterfaceClass);
	printf("subclass=0x%02X\n", data->bInterfaceSubClass);
	printf("protocol=0x%02X\n", data->bInterfaceProtocol);
	printf("index interface=0x%02X\n", data->iInterface);
}

void dumpEndpoint(UsbEndpointDescriptor* data, int i) {
	printf("\n");
	printf("ENDPOINT info %i\n",i);
	printf("--------------\n");
	printf("address=0x%02X\n", data->bEndpointAddress);	
	printf("attribs=0x%02X\n", data->bmAttributes);	
	printf("maxPacketsize=0x%02X%02X\n", data->wMaxPacketSizeHB, data->wMaxPacketSizeLB);	
	printf("interval=0x%02X\n", data->bInterval);	
}


void dumpInquiry(unsigned char * buf) {
	int i;

	printf("\n");
	printf("Inquiry data\n");
	printf("------------\n");
	printf("Vendor   : ");
	for (i = 8; i < 16; i++) printf("%c",buf[i]);
	printf("\n");

	printf("Prod id  :");
	for (i = 16; i < 32; i++) printf("%c",buf[i]);
	printf("\n");

	printf("Prod rev :");
	for (i = 32; i < 36; i++) printf("%c",buf[i]);

	printf("\n");
}

void dumpReadCapacity(unsigned char * buf) {
	int i;

	printf("\n");
	printf("Read capacity data\n");
	printf("------------------\n");
	printf("max LBA    :");
	for (i = 0; i < 4; i++) printf("%02X",buf[i]);
	printf("\n");

	printf("block size :");
	for (i = 4; i < 8; i++) printf("%02X",buf[i]);
	printf("\n");
}

#endif /* COMPILE_DUMPS */


/* reads one sector */
/*
int mass_stor_readSector1(unsigned int sector, unsigned char* buffer) {
	cbw_packet cbw;
	mass_dev* dev;
	int stat;
	int i;

	dev = &mass_device;

	// assume device is detected and configured - should be checked in upper levels 

	initCBWPacket(&cbw);

	cbw_scsi_read_sector(&cbw, sector, Size_Sector, 1);

	stat = 1;
	while (stat != 0) {
		XPRINTF("-READ SECTOR COMMAND: %i\n", sector);
		usb_bulk_command(dev, &cbw);

		XPRINTF("-READ SECTOR DATA\n");
		stat = usb_bulk_read(dev, buffer, Size_Sector); //change Hermes

       	XPRINTF("-READ SECTOR STATUS\n");
		stat = usb_bulk_manage_status(dev, -TAG_READ);
	}
	return Size_Sector;
}
*/

/* reads esctor group - up to 4096 bytes */
/* Modified by Hermes: read 4096 bytes */
int mass_stor_readSector4096(unsigned int sector, unsigned char* buffer) {
	cbw_packet cbw;
	int sectorSize;
	int stat;

	/* assume device is detected and configured - should be checked in upper levels */

	initCBWPacket(&cbw);
	sectorSize = Size_Sector;

	cbw_scsi_read_sector(&cbw, sector, sectorSize, 4096/sectorSize);  // Added by Hermes

	stat = 1;
	while (stat != 0) {
		XPRINTF("-READ SECTOR COMMAND: %i\n", sector);
		usb_bulk_command(&mass_device, &cbw);

		XPRINTF("-READ SECTOR DATA\n");
		stat = usb_bulk_transfer(mass_device.bulkEpI, buffer, 4096); //Modified by Hermes 

       	XPRINTF("-READ SECTOR STATUS\n");
		stat = usb_bulk_manage_status(&mass_device, -TAG_READ);
	}
	return 4096; //Modified by Hermes 
}

/* write sector group - up to 4096 bytes */
int mass_stor_writeSector4096(unsigned int sector, unsigned char* buffer) {
	cbw_packet cbw;
	int sectorSize;
	int stat;

	/* assume device is detected and configured - should be checked in upper levels */

	initCBWPacket(&cbw);
	sectorSize = Size_Sector;

	cbw_scsi_write_sector(&cbw, sector, sectorSize, 4096/sectorSize);  

	stat = 1;
	while (stat != 0) {
		XPRINTF("-WRITE SECTOR COMMAND: %i\n", sector);
		usb_bulk_command(&mass_device, &cbw);

		XPRINTF("-WRITE SECTOR DATA\n");
		stat = usb_bulk_transfer(mass_device.bulkEpO, buffer, 4096); 

       	XPRINTF("-READ SECTOR STATUS\n");
		stat = usb_bulk_manage_status(&mass_device, -TAG_WRITE);
	}
	return 4096; 
}


/* test that endpoint is bulk endpoint and if so, update device info */ 
void usb_bulk_probeEndpoint(mass_dev* dev, UsbEndpointDescriptor* endpoint) {
	if (endpoint->bmAttributes == USB_ENDPOINT_XFER_BULK) {
		/* out transfer */
		if ((endpoint->bEndpointAddress & 0x80) == 0 && dev->bulkEpO < 0) {
			dev->bulkEpOAddr = endpoint->bEndpointAddress;
			dev->bulkEpO = UsbOpenEndpointAligned(dev->devId, endpoint);
			dev->packetSzO = endpoint->wMaxPacketSizeHB * 256 + endpoint->wMaxPacketSizeLB;
			XPRINTF("register Output endpoint id =%i addr=%02X packetSize=%i\n", dev->bulkEpO,dev->bulkEpOAddr, dev->packetSzO);
		}else
		/* in transfer */
		if ((endpoint->bEndpointAddress & 0x80) != 0 && dev->bulkEpI < 0) {
			dev->bulkEpIAddr = endpoint->bEndpointAddress;
			dev->bulkEpI = UsbOpenEndpointAligned(dev->devId, endpoint);
			dev->packetSzI = endpoint->wMaxPacketSizeHB * 256 + endpoint->wMaxPacketSizeLB;
			XPRINTF("register Intput endpoint id =%i addr=%02X packetSize=%i\n", dev->bulkEpI, dev->bulkEpIAddr, dev->packetSzI);
		}
	}
}




int mass_stor_probe(int devId) {
	UsbDeviceDescriptor *device = NULL;        
	UsbConfigDescriptor *config = NULL;
	UsbInterfaceDescriptor *intf = NULL;

	/* only one device supported */
	if (mass_device.status & DEVICE_DETECTED) {
		XPRINTF("mass_stor: Error: only one mass storage device allowed ! \n");
		return 0;
	}

	/* get device descriptor */
	device = (UsbDeviceDescriptor*)UsbGetDeviceStaticDescriptor(devId, NULL, USB_DT_DEVICE);
	if (device == NULL)  {
		printf("mass_stor: Error: Couldn't get device descriptor\n");
		return 0;
	}
#ifdef DEBUG
	dumpDevice(device);
#endif

	/* Check if the device has at least one configuration */
	if(device->bNumConfigurations < 1) {
	      return 0;
	}
	/* read configuration */
	config = (UsbConfigDescriptor*)UsbGetDeviceStaticDescriptor(devId, device, USB_DT_CONFIG);
	if (config == NULL) {
	      printf("ERROR: Couldn't get configuration descriptor\n");
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
	//dumpInterface(intf);
	/*
	XPRINTF("Interface Length %d Endpoints %d Class %d Sub %d Proto %d\n", intf->bLength, 
	 intf->bNumEndpoints, intf->bInterfaceClass, intf->bInterfaceSubClass, 
	 intf->bInterfaceProtocol); 
	*/

	if(	(intf->bInterfaceClass		!= USB_CLASS_MASS_STORAGE) ||
		(intf->bInterfaceSubClass	!= USB_SUBCLASS_MASS_SCSI  &&
		 intf->bInterfaceSubClass	!= USB_SUBCLASS_MASS_SFF_8070I) ||
		(intf->bInterfaceProtocol	!= USB_PROTOCOL_MASS_BULK_ONLY) ||
		(intf->bNumEndpoints < 2))    { //one bulk endpoint is not enough because
			 return 0;		//we send the CBW to te bulk out endpoint
	}
	return 1;
}

static int sif_send_cmd ( int anID, int anArg ) {  // from audsrv

 static int lCmdData[ 4 ] __attribute__(   (  aligned( 64 )  )   );

 lCmdData[ 3 ] = anArg;

 return sceSifSendCmd ( anID, lCmdData, 16, NULL, NULL, 0 );

}  /* end sif_send_cmd */

int mass_stor_connect(int devId) {
	int i;
	int epCount;
	UsbDeviceDescriptor *device;        
	UsbConfigDescriptor *config;
	UsbInterfaceDescriptor *interface;
	UsbEndpointDescriptor *endpoint;
	u16 number;
	mass_dev* dev;
	
	printf ("usb_mass: connect: devId=%i\n", devId);
	dev = &mass_device;
	
	/* only one mass device allowed */
	if (dev->status & DEVICE_DETECTED) {
		printf("usb_mass: Error - only one mass storage device allowed !\n");
		return 1;
	}

	dev->devId = devId;
	dev->bulkEpI = -1;
	dev->bulkEpO = -1;
	dev->controlEp = -1;

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
	usb_bulk_probeEndpoint(dev, endpoint);
	
	for (i = 1; i < epCount; i++) {
		endpoint = (UsbEndpointDescriptor*) ((char *) endpoint + endpoint->bLength);
		usb_bulk_probeEndpoint(dev, endpoint);
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

	/* 
		no usb commands (other than getDeviceDescriptor) can be called 
		in this connection state or else the callback function is never 
		called (usbd feature or bug?). So we have to confirm this 
		connection (return 0) and then we are free to call any usbd functions.
	*/

	/*store current configuration id - can't call set_configuration here */
	dev->configId = config->bConfigurationValue;
	dev->status += DEVICE_DETECTED;
	printf("usb_mass: connect ok: epI=%i, epO=%i \n", dev->bulkEpI, dev->bulkEpO);

      sif_send_cmd ( MASS_CONNECT_CALLBACK, 0 );

	return 0;
}

void mass_stor_release() {
	mass_dev* dev;
	dev = &mass_device;

	if (dev->bulkEpI >= 0) {
		UsbCloseEndpoint(dev->bulkEpI);
	}
	if (dev->bulkEpO >= 0) {
		UsbCloseEndpoint(dev->bulkEpO);
	}
	dev->devId = -1;
	dev->bulkEpI = -1;
	dev->bulkEpO = -1;
	dev->controlEp = -1;
	dev->status = DEVICE_DISCONNECTED;

}

int mass_stor_disconnect(int devId) {
	mass_dev* dev;

	dev = &mass_device;
	printf ("usb_mass: disconnect: devId=%i\n", devId);

	if ((dev->status & DEVICE_DETECTED) && devId == dev->devId) {
		mass_stor_release();
            sif_send_cmd ( MASS_DISCONNECT_CALLBACK, 0 );
	}
	return 0;
}


int mass_stor_warmup() {
	cbw_packet cbw;
	unsigned char buffer[64];
	mass_dev* dev;
	int sectorSize;
	int stat;
	int retryCount;
	int i;

	dev = &mass_device;

	XPRINTF("mass_stor: warm up called ! \n");
	if (!(dev->status & DEVICE_DETECTED)) {
		printf("usb_mass: Error - no mass storage device found!\n");
		return -1;
	}


	initCBWPacket(&cbw);

	//send inquiry command
	memset(buffer, 0, 64);
	cbw_scsi_inquiry(&cbw);
	XPRINTF("-INQUIRY\n");
	usb_bulk_command(dev, &cbw);

	XPRINTF("-INQUIRY READ DATA\n");
	usb_bulk_transfer(dev->bulkEpI, buffer, 36);

	XPRINTF("-INQUIRY STATUS\n");
	residue = 0;
	usb_bulk_manage_status(dev, -TAG_INQUIRY);

#ifdef DEBUG
	dumpInquiry(buffer);
#endif
	if (returnSize <= 0) {
		printf("!Error: device not ready!\n");
		return -1;
	} 
/*
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
	usb_bulk_manage_status(dev, -TAG_TEST_UNIT_READY);
*/

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
		usb_bulk_transfer(dev->bulkEpI, buffer, 8);

		//HACK HACK HACK !!!
		//according to usb doc we should allways 
		//attempt to read the CSW packet. But in some cases
		//reading of CSW packet just freeze ps2....:-(
		if (returnCode == 4) {
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
	// Added by Hermes
	Size_Sector = getBI32 ( &buffer[ 4 ] );
    g_MaxLBA    = getBI32 ( &buffer[ 0 ] );
//	printf("PHYSICAL SECTOR SIZE: 0x%x\n",Size_Sector);
//    printf("MAX LBA: 0x%X\n", g_MaxLBA );

#ifdef DEBUG
	dumpReadCapacity(buffer);	
#endif
	return 0;

}

int mass_stor_getStatus() {
	int i;
	XPRINTF("mass_stor: getting status... \n");
	if (!(mass_device.status & DEVICE_DETECTED)) {
		XPRINTF("usb_mass: Error - no mass storage device found!\n");
		return -1;
	}
	if (!(mass_device.status & DEVICE_CONFIGURED)) {
		//usb_bulk_reset(&mass_device, 1);
		set_configuration(&mass_device, mass_device.configId);
		//maybe this wait is not necessary, but who knows....
		for (i = 0; i < 0xFFFFF; i++) asm("nop\nnop\nnop\nnop");	
		set_interface(&mass_device, mass_device.interfaceNumber, mass_device.interfaceAlt);
		mass_device.status += DEVICE_CONFIGURED;
		i= mass_stor_warmup();
		if (i < 0) {
			mass_stor_release();
			return i;
		}
	}
	return mass_device.status;
}

int mass_stor_init() {
	int ret;
	mass_device.status = 0;

	driver.next 		= NULL;
	driver.prev		= NULL;	
	driver.name 		= "mass-stor";
	driver.probe 		= mass_stor_probe; 
	driver.connect		= mass_stor_connect;
	driver.disconnect	= mass_stor_disconnect;

	ret = UsbRegisterDriver(&driver);
	XPRINTF("mass_stor: registerDriver=%i \n", ret);
	if (ret < 0) {
		printf("usb_mass: register driver failed! ret=%d\n", ret);
	}
}

