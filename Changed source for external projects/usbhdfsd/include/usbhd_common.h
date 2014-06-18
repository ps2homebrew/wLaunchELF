#ifndef _USBHD_COMMON_H
#define _USBHD_COMMON_H

#include <io_common.h>
#include <ioman.h>
#include "fat.h"

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

#define getBI32(__buf) ((((u8 *) (__buf))[3] << 0) | (((u8 *) (__buf))[2] << 8) | (((u8 *) (__buf))[1] << 16) | (((u8 *) (__buf))[0] << 24))

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

typedef struct _fs_rec {
	int           file_flag;
  //This flag is always 1 for a file, and always 0 for a folder (different typedef)
  //Routines that handle both must test it, and then typecast the privdata pointer
  //to the type that is appropriate for the given case. (see also D_PRIVATE typedef)
	int           fd;
	unsigned int  filePos;
	int           mode;	//file open mode
	unsigned int  sfnSector; //short filename sector  - write support
	int           sfnOffset; //short filename offset  - write support
	int           sizeChange; //flag
} fs_rec;


#define FAT_ERROR           -1

#define MAX_DIR_CLUSTER 512


int mass_stor_getStatus();

int fs_init   (iop_device_t *driver); 
int fs_open   (iop_file_t* , const char *name, int mode, ...);
int fs_lseek  (iop_file_t* , unsigned long offset, int whence);
int fs_read   (iop_file_t* , void * buffer, int size );
int fs_write  (iop_file_t* , void * buffer, int size );
int fs_close  (iop_file_t* );
int fs_dummy  (void);

int fs_deinit (iop_device_t *);
int fs_format (iop_file_t *, ...);
int fs_ioctl  (iop_file_t *, unsigned long, void *);
int fs_remove (iop_file_t *, const char *);
int fs_mkdir  (iop_file_t *, const char *);
int fs_rmdir  (iop_file_t *, const char *);
int fs_dopen  (iop_file_t *, const char *);
int fs_dclose (iop_file_t *);
int fs_dread  (iop_file_t *, void *);
int fs_getstat(iop_file_t *, const char *, void *);

int fs_chstat (iop_file_t *, const char *, void *, unsigned int);


int getI32(unsigned char* buf);
int getI32_2(unsigned char* buf1, unsigned char* buf2);
int getI16(unsigned char* buf);
int strEqual(unsigned char *s1, unsigned char* s2);
unsigned int fat_getClusterRecord12(unsigned char* buf, int type);
unsigned int fat_cluster2sector(fat_bpb* bpb, unsigned int cluster);

int      fat_initDriver(void);
void     fat_closeDriver(void);
fat_bpb* fat_getBpb(void);
int      fat_getFileStartCluster(fat_bpb* bpb, const char* fname, unsigned int* startCluster, fat_dir* fatDir);
int      fat_getDirentrySectorData(fat_bpb* bpb, unsigned int* startCluster, unsigned int* startSector, int* dirSector);
unsigned int fat_cluster2sector(fat_bpb* bpb, unsigned int cluster);
int      fat_getDirentry(fat_direntry_sfn* dsfn, fat_direntry_lfn* dlfn, fat_direntry* dir );
int      fat_getClusterChain(fat_bpb* bpb, unsigned int cluster, unsigned int* buf, int bufSize, int start);
void     fat_invalidateLastChainResult();
void     fat_getClusterAtFilePos(fat_bpb* bpb, fat_dir* fatDir, unsigned int filePos, unsigned int* cluster, unsigned int* clusterPos);

#endif // _USBHD_COMMON_H
