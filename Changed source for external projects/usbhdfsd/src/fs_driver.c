//---------------------------------------------------------------------------
//File name:    fs_driver.c
//---------------------------------------------------------------------------
/*
 * fat_driver.c - USB Mass storage driver for PS2
 *
 * (C) 2004, Marek Olejnik (ole00@post.cz)
 * (C) 2004  Hermes (support for sector sizes from 512 to 4096 bytes)
 * (C) 2004  raipsu (fs_dopen, fs_dclose, fs_dread, fs_getstat implementation)
 *
 * FAT filesystem layer
 *
 * See the file LICENSE included with this distribution for licensing terms.
 */

#include <stdio.h>
#include <sysclib.h>
#include <thsemap.h>
#include <sys/stat.h>

#include <thbase.h>
#include <errno.h>
#include <sysmem.h>
#define malloc(a)       AllocSysMemory(0,(a), NULL)
#define free(a)         FreeSysMemory((a))

#include "scache.h"
#include "fat_driver.h"
#include "fat_write.h"
#include "usbhd_common.h"

//#define DEBUG  //comment out this line when not debugging

#include "mass_debug.h"

#define IOCTL_CKFREE 0xBABEC0DE  //dlanor: Ioctl request code => Check free space
#define IOCTL_RENAME 0xFEEDC0DE  //dlanor: Ioctl request code => Rename

#define FLUSH_SECTORS		scache_flushSectors

#define MAX_FILES 16
fs_rec  fsRec[MAX_FILES]; //file info record
fat_dir fsDir[MAX_FILES]; //directory entry
static int fsCounter = 0;

extern fat_part partTable;	//partition master record
extern fat_bpb  partBpb;	//partition bios parameter block

static iop_device_t fs_driver;
static iop_device_ops_t fs_functarray;

/* init file system driver */
int InitFS()
{
	fs_driver.name = "mass";
	fs_driver.type = IOP_DT_FS;
	fs_driver.version = 1;
	fs_driver.desc = "Usb mass storage driver";
	fs_driver.ops = &fs_functarray;

	fs_functarray.init    = fs_init;
	fs_functarray.deinit  = fs_deinit;
	fs_functarray.format  = fs_format;
	fs_functarray.open    = fs_open;
	fs_functarray.close   = fs_close;
	fs_functarray.read    = fs_read;
	fs_functarray.write   = fs_write;
	fs_functarray.lseek   = fs_lseek;
	fs_functarray.ioctl   = fs_ioctl;
	fs_functarray.remove  = fs_remove;
	fs_functarray.mkdir   = fs_mkdir;
	fs_functarray.rmdir   = fs_rmdir;
	fs_functarray.dopen   = fs_dopen;
	fs_functarray.dclose  = fs_dclose;
	fs_functarray.dread   = fs_dread;
	fs_functarray.getstat = fs_getstat;
	fs_functarray.chstat  = fs_chstat;

	DelDrv("mass");
	AddDrv(&fs_driver);

	// should return an error code if AddDrv fails...
	return(0);
}


/*************************************************************************************/
/*    File IO functions                                                              */
/*************************************************************************************/


int	nameSignature;
int	removalTime;
int     removalResult;

typedef struct {
	int           file_flag;
  //This flag is always 1 for a file, and always 0 for a folder (different typedef)
  //Routines that handle both must test it, and then typecast the privdata pointer
  //to the type that is appropriate for the given case. (see also fs_rec typedef)
   int status;
   fat_dir fatdir;
} D_PRIVATE;


//---------------------------------------------------------------------------
int fs_findFreeFileSlot(int fd) {
	int i;
	int result = -1;
	for (i = 0; i < MAX_FILES; i++) {
		if (fsRec[i].fd == fd) {
			result = i;
			break;
		}
	}
	return result;
}

//---------------------------------------------------------------------------
int fs_findFileSlot(iop_file_t* file) {
	int i;
	int result = -1;
	fs_rec* rec = (fs_rec*)file->privdata;
	int fd = rec->fd;

	for (i = 0; i < MAX_FILES; i++) {
		if (fsRec[i].fd == fd) {
			result = i;
			break;
		}
	}
	return result;
}

//---------------------------------------------------------------------------
int fs_findFileSlotByName(const char* name) {
	int i;
	int result = -1;

	for (i = 0; i < MAX_FILES; i++) {
		if (fsRec[i].fd >= 0 && strEqual(fsDir[i].name, (unsigned char*) name) == 0) {
			result = i;
			break;
		}
	}
	return result;
}

//---------------------------------------------------------------------------
static int _lock_sema_id = 0;

//---------------------------------------------------------------------------
int _fs_init_lock(void)
{
	iop_sema_t sp;

	sp.initial = 1;
	sp.max = 1;
	sp.option = 0;
	sp.attr = 0;
	if((_lock_sema_id = CreateSema(&sp)) < 0) { return(-1); }

	return(0);
}

//---------------------------------------------------------------------------
int _fs_lock(void)
{
    if(WaitSema(_lock_sema_id) != _lock_sema_id) { return(-1); }
    return(0);
}

//---------------------------------------------------------------------------
int _fs_unlock(void)
{
    SignalSema(_lock_sema_id);
    return(0);
}

//---------------------------------------------------------------------------
void fs_reset(void)
{
	int i;
	for (i = 0; i < MAX_FILES; i++) { fsRec[i].fd = -1; }
	if(_lock_sema_id >= 0) { DeleteSema(_lock_sema_id); }
	_fs_init_lock();
}

//---------------------------------------------------------------------------
int fs_inited = 0;

//---------------------------------------------------------------------------
int fs_init(iop_device_t *driver)
{
    if(fs_inited) { return(1); }

	fs_reset();

    fs_inited = 1;
	return 1;
}

//---------------------------------------------------------------------------
int fs_open(iop_file_t* fd, const char *name, int mode, ...) {
	int index, index2;
	int mode2;
	int ret;
	unsigned int cluster;
	char escapeNotExist;

	_fs_lock();

	XPRINTF("fs_open called: %s mode=%X \n", name, mode) ;

	//check if media mounted
	ret = fat_mountCheck();
	if (ret < 0) { _fs_unlock(); return ret; }

	//check if the slot is free
	index = fs_findFreeFileSlot(-1);
	if (index  < 0) { _fs_unlock(); return -EMFILE; }
	fsRec[index].file_flag = 1;

	//check if the file is already open
	index2 = fs_findFileSlotByName(name);

	//file already open
	if (index2 >= 0) {
		mode2 = fsRec[index2].mode;
		if (	(mode & O_WRONLY) || //current file is opened for write
			(mode2 & O_WRONLY) ) {//other file is opened for write
			_fs_unlock();
			return 	-EACCES;
		}
	}

	if((mode & O_WRONLY))  { //dlanor: corrected bad test condition
		cluster = 0; //start from root

		escapeNotExist = 1;
		if (mode & O_CREAT) {
			XPRINTF("FAT I: O_CREAT detected!\n");
			escapeNotExist = 0;
		}

		fsRec[index].sfnSector = 0;
		fsRec[index].sfnOffset = 0;
		ret = fat_createFile(&partBpb, name, 0, escapeNotExist, &cluster, &fsRec[index].sfnSector, &fsRec[index].sfnOffset);
		if (ret < 0) {
		    _fs_unlock(); 
			return ret;
		}
		//the file already exist but mode is set to truncate
		if (ret == 2 && (mode & O_TRUNC)) {
			XPRINTF("FAT I: O_TRUNC detected!\n");
			fat_truncateFile(&partBpb, cluster, fsRec[index].sfnSector, fsRec[index].sfnOffset);
		}
	}

	//find the file
	cluster = 0; //allways start from root
	XPRINTF("Calling fat_getFileStartCluster from fs_open\n");
	ret = fat_getFileStartCluster(&partBpb, name, &cluster, &fsDir[index]);
	if (ret < 0) {
	    _fs_unlock(); 
		return ret;
	}

	//store fd to file slot
	fsCounter++;
	fsRec[index].fd = fsCounter; //fd
	fsRec[index].mode = mode;
	fsRec[index].filePos = 0;
	fsRec[index].sizeChange  = 0;

	if ((mode & O_APPEND) && (mode & O_WRONLY)) {
		XPRINTF("FAT I: O_APPEND detected!\n");
		fsRec[index].filePos = fsDir[index].size;
	}

	//store the slot to user parameters
	fd->privdata = &fsRec[index];

    _fs_unlock();
	return fsCounter;
}

//---------------------------------------------------------------------------
int fs_close(iop_file_t* fd) {
	int index;
    int ret;

    _fs_lock();

    //check if media mounted
    ret = fat_mountCheck();
    if (ret < 0) { _fs_unlock(); return ret; }

	index = fs_findFileSlot(fd);
	if (index < 0) {
		_fs_unlock();
		return -EFAULT;
	}
	fsRec[index].fd = -1;

	if ((fsRec[index].mode & O_WRONLY)) {
		//update direntry size and time
		if (fsRec[index].sizeChange) {
			fat_updateSfn(fsDir[index].size, fsRec[index].sfnSector, fsRec[index].sfnOffset);
		}

		FLUSH_SECTORS();
	}

    _fs_unlock();
	return 0;
}

//---------------------------------------------------------------------------
int fs_lseek(iop_file_t* fd, unsigned long offset, int whence) {
	int index;
    int ret;

    _fs_lock();

    //check if media mounted
    ret = fat_mountCheck();
    if (ret < 0) { _fs_unlock(); return ret; }

	index = fs_findFileSlot(fd);
	if (index < 0) {
        _fs_unlock();
		return -EFAULT;
	}
	switch(whence) {
		case SEEK_SET:
			fsRec[index].filePos = offset;
			break;
		case SEEK_CUR:
			fsRec[index].filePos += offset;
			break;
		case SEEK_END:
			fsRec[index].filePos = fsDir[index].size + offset;
			break;
		default:
		    _fs_unlock();
			return -1;
	}
	if (fsRec[index].filePos < 0) {
		fsRec[index].filePos = 0;
	}
	if (fsRec[index].filePos > fsDir[index].size) {
		fsRec[index].filePos = fsDir[index].size;
	}

	_fs_unlock();
	return fsRec[index].filePos;
}

//---------------------------------------------------------------------------
int fs_write(iop_file_t* fd, void * buffer, int size )
{
	int index;
	int result;
	int updateClusterIndices;
    int ret;

    _fs_lock();

    //check if media mounted
    ret = fat_mountCheck();
    if (ret < 0) { _fs_unlock(); return ret; }

	updateClusterIndices = 0;

	index = fs_findFileSlot(fd);
	if (index < 0) { _fs_unlock(); return -1; }
	if (size <= 0) { _fs_unlock(); return 0; }

	result = fat_writeFile(&partBpb, &fsDir[index], &updateClusterIndices, fsRec[index].filePos, (unsigned char*) buffer, size);
	if (result > 0) { //write succesful
		fsRec[index].filePos += result;
		if (fsRec[index].filePos > fsDir[index].size) {
			fsDir[index].size = fsRec[index].filePos;
			fsRec[index].sizeChange = 1;
			//if new clusters allocated - then update file cluster indices
			if (updateClusterIndices) {
				fat_setFatDirChain(&partBpb, &fsDir[index]);
			}
		}
	}
	
	_fs_unlock();
	return result;
}

//---------------------------------------------------------------------------
int fs_read(iop_file_t* fd, void * buffer, int size ) {
	int index;
	int result;
    int ret;

    _fs_lock();

    //check if media mounted
    ret = fat_mountCheck();
    if (ret < 0) { _fs_unlock(); return ret; }

	index = fs_findFileSlot(fd);
	if (index < 0) {
	    _fs_unlock();
		return -1;
	}

	if (size<=0) {
	    _fs_unlock();
		return 0;
	}

	if ((fsRec[index].filePos+size) > fsDir[index].size) {
		size = fsDir[index].size - fsRec[index].filePos;
	}

	result = fat_readFile(&partBpb, &fsDir[index], fsRec[index].filePos, (unsigned char*) buffer, size);
	if (result > 0) { //read succesful
		fsRec[index].filePos += result;
	}

	_fs_unlock();
	return result;
}


//---------------------------------------------------------------------------
int getNameSignature(const char *name) {
	int ret;
	int i;
	ret = 0;

	for (i=0; name[i] != 0 ; i++) ret += (name[i]*i/2 + name[i]);
	return ret;
}

//---------------------------------------------------------------------------
int getMillis()
{
	iop_sys_clock_t clock;
	u32 sec, usec;

	GetSystemTime(&clock);
	SysClock2USec(&clock, &sec, &usec);
	return   (sec*1000) + (usec/1000);
}

//---------------------------------------------------------------------------
int fs_remove (iop_file_t *fd, const char *name) {
	int index;
	int result;
    int ret;

    _fs_lock();

    //check if media mounted
    ret = fat_mountCheck();
    if (ret < 0)
    {
		result = -1;
		removalResult = result;
        _fs_unlock();
 		return result;
	}

	index = fs_findFileSlotByName(name);
	//store filename signature and time of removal
	nameSignature = getNameSignature(name);
	removalTime = getMillis();

	//file is opened - can't delete the file
	if (index >= 0) {
		result = -EINVAL;
		removalResult = result;
        _fs_unlock();
		return result;
	}

	result = fat_deleteFile(&partBpb, name, 0);
	FLUSH_SECTORS();
	removalTime = getMillis(); //update removal time
	removalResult = result;

    _fs_unlock();
	return result;
}

//---------------------------------------------------------------------------
int fs_mkdir  (iop_file_t *fd, const char *name) {
	int ret;
	int sfnOffset;
	unsigned int sfnSector;
	unsigned int cluster;
	int sig, millis;

    _fs_lock();

    //check if media mounted
    ret = fat_mountCheck();
    if (ret < 0) { _fs_unlock(); return ret; }

	XPRINTF("fs_mkdir: name=%s \n",name);
	//workaround for bug that invokes fioMkdir right after fioRemove
	sig = getNameSignature(name);
	millis = getMillis();
	if (sig == nameSignature && (millis - removalTime) < 1000) {
        _fs_unlock();
		return removalResult; //return the original return code from fs_remove
	}

	ret = fat_createFile(&partBpb, name, 1, 0, &cluster,  &sfnSector, &sfnOffset);

	//directory of the same name already exist
	if (ret == 2) {
		ret = -EEXIST;
	}
	FLUSH_SECTORS();

    _fs_unlock();
	return ret;
}

//---------------------------------------------------------------------------
// NOTE! you MUST call fioRmdir with device number in the name
// or else this fs_rmdir function is never called.
// example: fioRmdir("mass:/dir1");  //    <-- doesn't work (bug?)
//          fioRmdir("mass0:/dir1"); //    <-- works fine
int fs_rmdir  (iop_file_t *fd, const char *name) {
	int ret;

    _fs_lock();

    //check if media mounted
    ret = fat_mountCheck();
    if (ret < 0) { _fs_unlock(); return ret; }

	ret = fat_deleteFile(&partBpb, name, 1);
	FLUSH_SECTORS();
    _fs_unlock();
	return ret;
}

//---------------------------------------------------------------------------
int fs_dopen  (iop_file_t *fd, const char *name)
{
	//int index;
	fat_dir fatdir;
	int ret, is_root = 0;

	_fs_lock();

	//check if media mounted
	ret = fat_mountCheck();
	if (ret < 0) { _fs_unlock(); return ret; }

	fsCounter++;

	if( ((name[0] == '/') && (name[1] == '\0'))
		||((name[0] == '/') && (name[1] == '.') && (name[2] == '\0')))
	{
		name = "/";
		is_root = 1;
	}

	ret = fat_getFirstDirentry((char*)name, &fatdir);

	fd->privdata = (void*)malloc(sizeof(D_PRIVATE));
	memset(fd->privdata, 0, sizeof(D_PRIVATE)); //NB: also implies "file_flag = 0;"

	if (ret < 1)
	{
		// root directory may have no entries, nothing else may.
		if(!is_root) { free(fd->privdata); _fs_unlock(); return(-1); }

		// cause "dread" to return "no entries" immediately since dir is empty.
		((D_PRIVATE *) fd->privdata)->status = 1;
	}

	memcpy(&(((D_PRIVATE*)fd->privdata)->fatdir), &fatdir, sizeof(fatdir));

	_fs_unlock();
	return fsCounter;
}

//---------------------------------------------------------------------------
int fs_dclose (iop_file_t *fd)
{
	_fs_lock();
	free(fd->privdata);
	_fs_unlock();
	return 0;
}

//---------------------------------------------------------------------------
int fs_dread  (iop_file_t *fd, void* data)
{
	int notgood;
	int ret;

	_fs_lock();

	//check if media mounted
	ret = fat_mountCheck();
	if (ret < 0) { _fs_unlock(); return ret; }

	fio_dirent_t *buffer = (fio_dirent_t *)data;
	do {
		if (((D_PRIVATE*)fd->privdata)->status)
		{
			_fs_unlock();
			return 0;
		}

		notgood = 0;

		memset(buffer, 0, sizeof(fio_dirent_t));
		if ((((D_PRIVATE*)fd->privdata)->fatdir).attr & 0x08) {	 /* volume name */
			notgood = 1;
		}
		if ((((D_PRIVATE*)fd->privdata)->fatdir).attr & 0x10) {
			buffer->stat.mode |= FIO_SO_IFDIR;
		} else {
			buffer->stat.mode |= FIO_SO_IFREG;
		}

		buffer->stat.size = (((D_PRIVATE*)fd->privdata)->fatdir).size;

		strcpy(buffer->name, (const char*)(((D_PRIVATE*)fd->privdata)->fatdir).name);

		//set created Date: Day, Month, Year
		buffer->stat.ctime[4] = (((D_PRIVATE*)fd->privdata)->fatdir).cdate[0];
		buffer->stat.ctime[5] = (((D_PRIVATE*)fd->privdata)->fatdir).cdate[1];
		buffer->stat.ctime[6] = (((D_PRIVATE*)fd->privdata)->fatdir).cdate[2];
		buffer->stat.ctime[7] = (((D_PRIVATE*)fd->privdata)->fatdir).cdate[3];

		//set created Time: Hours, Minutes, Seconds
		buffer->stat.ctime[3] = (((D_PRIVATE*)fd->privdata)->fatdir).ctime[0];
		buffer->stat.ctime[2] = (((D_PRIVATE*)fd->privdata)->fatdir).ctime[1];
		buffer->stat.ctime[1] = (((D_PRIVATE*)fd->privdata)->fatdir).ctime[2];

 		//set accessed Date: Day, Month, Year
		buffer->stat.atime[4] = (((D_PRIVATE*)fd->privdata)->fatdir).adate[0];
		buffer->stat.atime[5] = (((D_PRIVATE*)fd->privdata)->fatdir).adate[1];
		buffer->stat.atime[6] = (((D_PRIVATE*)fd->privdata)->fatdir).adate[2];
		buffer->stat.atime[7] = (((D_PRIVATE*)fd->privdata)->fatdir).adate[3];

		//set modified Date: Day, Month, Year
		buffer->stat.mtime[4] = (((D_PRIVATE*)fd->privdata)->fatdir).mdate[0];
		buffer->stat.mtime[5] = (((D_PRIVATE*)fd->privdata)->fatdir).mdate[1];
		buffer->stat.mtime[6] = (((D_PRIVATE*)fd->privdata)->fatdir).mdate[2];
		buffer->stat.mtime[7] = (((D_PRIVATE*)fd->privdata)->fatdir).mdate[3];
 
		//set modified Time: Hours, Minutes, Seconds
		buffer->stat.mtime[3] = (((D_PRIVATE*)fd->privdata)->fatdir).mtime[0];
		buffer->stat.mtime[2] = (((D_PRIVATE*)fd->privdata)->fatdir).mtime[1];
		buffer->stat.mtime[1] = (((D_PRIVATE*)fd->privdata)->fatdir).mtime[2];

		if (fat_getNextDirentry(&(((D_PRIVATE*)fd->privdata)->fatdir))<1)
			((D_PRIVATE*)fd->privdata)->status = 1;	/* no more entries */
	} while (notgood);

	_fs_unlock();
	return 1;
}

//---------------------------------------------------------------------------
int fs_getstat(iop_file_t *fd, const char *name, void* data)
{
	int ret;
	unsigned int cluster = 0;
	fio_stat_t *stat = (fio_stat_t *)data;
	fat_dir fatdir;

	_fs_lock();

	//check if media mounted
	ret = fat_mountCheck();
	if (ret < 0) { _fs_unlock(); return ret; }

	XPRINTF("Calling fat_getFileStartCluster from fs_getstat\n");
	ret = fat_getFileStartCluster(&partBpb, name, &cluster, &fatdir);
	if (ret < 0) {
		_fs_unlock();
		return -1;
	}

	memset(stat, 0, sizeof(fio_stat_t));
	stat->size = fatdir.size;
	if (fatdir.attr & 10)
		stat->mode |= FIO_SO_IFDIR;
	else
		stat->mode |= FIO_SO_IFREG;

	_fs_unlock();
	return 0;
}

//---------------------------------------------------------------------------
int fs_chstat (iop_file_t *fd, const char *name, void *buffer, unsigned int a)
{
	return fs_dummy();
}

//---------------------------------------------------------------------------
int fs_deinit (iop_device_t *fd)
{
	return fs_dummy();
}

//---------------------------------------------------------------------------
int fs_format (iop_file_t *fd, ...)
{
	return fs_dummy();
}

//---------------------------------------------------------------------------
int fs_ioctl  (iop_file_t *fd, unsigned long request, void *data)
{
	int ret;

	_fs_lock();
	//check if media mounted
	ret = fat_mountCheck();
	if (ret < 0) goto return_ret;

	switch (request) {
		case IOCTL_CKFREE:  //Request to calculate free space (ignore file/folder selected)
			ret = fs_dummy();
			break;
		case IOCTL_RENAME:  //Request to rename opened file/folder
			ret = fs_dummy();
			break;
		default:
			ret = fs_dummy();
	}
return_ret:
	_fs_unlock();
	return ret;
}

//---------------------------------------------------------------------------
int fs_dummy(void)
{
	return -5;
}
//---------------------------------------------------------------------------
//End of file:  fs_driver.c
//---------------------------------------------------------------------------
