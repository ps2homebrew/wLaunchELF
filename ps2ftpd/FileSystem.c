/*
 * FileSystem.c - FTP server filesystem layer
 *
 * Copyright (C) 2004 Jesper Svennevid
 *
 * Device scan is based on the one from ps2netfs.
 *
 * Licensed under the AFL v2.0. See the file LICENSE included with this
 * distribution for licensing terms.
 *
 */

#include "FileSystem.h"

#ifndef LINUX
#include "irx_imports.h"
#define assert(x)
#define isnum(c) ((c) >= '0' && (c) <= '9')
#else
#include <assert.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#endif

// present in FtpClient.c
extern char* itoa(char* in, int val);

// these offsets depend on the layout of both ioman & iomanX ... ps2netfs does
// something similar, so if it breaks, we all go down.
#define DEVINFO_IOMAN_OFFSET 0x0c
#define DEVINFO_IOMANX_OFFSET 0

#define DEVINFOARRAY(d,ofs) ((iop_device_t **)((d)->text_start + (d)->text_size + (d)->data_size + (ofs)))

// buffer used for concating filenames internally
static char buffer[512];

void FileSystem_Create( FSContext* pContext )
{
	strcpy(pContext->m_Path,"/");

#ifndef LINUX
	pContext->m_eType = FS_INVALID;
	memset( &(pContext->m_kFile), 0, sizeof(pContext->m_kFile) );
#else
	pContext->m_iFile = -1;
	pContext->m_pDir = NULL;
#endif
}

void FileSystem_Destroy( FSContext* pContext )
{
	FileSystem_Close(pContext);
}

int FileSystem_OpenFile( FSContext* pContext, const char* pFile, FileMode eMode, int iContinue )
{
	int flags;
	int fileMode = 0;

	FileSystem_Close(pContext);
	FileSystem_BuildPath( buffer, pContext->m_Path, pFile );

	switch( eMode )
	{
		case FM_READ: flags = O_RDONLY; break;
		case FM_WRITE: flags = O_CREAT|O_TRUNC|O_WRONLY; break;

		default:
			return -1;
	}

#ifdef LINUX
	if( (pContext->m_iFile = open(buffer,flags,S_IRWXU|S_IRGRP)) < 0 )
		return -1;

	return 0;
#else
	if( NULL == (pFile = FileSystem_ClassifyPath( pContext, buffer )) )
		return -1;

	switch( pContext->m_eType )
	{
		case FS_IODEVICE:
		{
			if( iContinue )
			{
				int iOpened = 0;

				if( flags & O_WRONLY )
				{
					pContext->m_kFile.mode = O_WRONLY;
					if( pContext->m_kFile.device->ops->open( &(pContext->m_kFile), pFile, pContext->m_kFile.mode, 0 ) >= 0 )
						iOpened = 1;
				}
				else
				{
					pContext->m_kFile.mode = O_RDONLY;
					if( pContext->m_kFile.device->ops->open( &(pContext->m_kFile), pFile, pContext->m_kFile.mode, 0 ) >= 0 )
						iOpened = 1;
				}

				// seek to position if we successfully opened the file

				if( iOpened )
				{
					if( iContinue < 0 )
					{
						if( pContext->m_kFile.device->ops->lseek( &(pContext->m_kFile), 0, SEEK_END ) >= 0 )
							return 0;
					}
					else
					{
						if( pContext->m_kFile.device->ops->lseek( &(pContext->m_kFile), iContinue, SEEK_SET ) >= 0 )
							return 0;
					}

					// could not seek, close file and open normally
					pContext->m_kFile.device->ops->close( &(pContext->m_kFile) );
				}
			}

			pContext->m_kFile.mode = flags;

			// check if hdd, then set permissions to 511
			if( !strcmp(pContext->m_kFile.device->name, "pfs") )
				fileMode = 511;

			if( pContext->m_kFile.device->ops->open( &(pContext->m_kFile), pFile, flags, fileMode ) >= 0 )
				return 0;

		}
		break;

		default:
			return -1;
	}

	pContext->m_eType = FS_INVALID;
	return -1;
#endif
}

int FileSystem_OpenDir( FSContext* pContext, const char* pDir )
{
	FileSystem_Close(pContext);
	FileSystem_BuildPath( buffer, pContext->m_Path, pDir );

#ifdef LINUX

	strcpy(pContext->m_List,buffer);

	// unsafe, sure
	pContext->m_pDir = opendir(pContext->m_List);
	return pContext->m_pDir ? 0 : -1;

#else

	if( NULL == (pDir = FileSystem_ClassifyPath( pContext, buffer )) )
		return -1;

	switch( pContext->m_eType )
	{
		// directory listing from I/O device
		case FS_IODEVICE:
		{
			if( !pContext->m_kFile.device )
				break;

			// attempt to open device directory

			pContext->m_kFile.mode = O_DIROPEN;
			if( pContext->m_kFile.device->ops->dopen( &(pContext->m_kFile), pDir ) >=0 )
				return 0;
		}
		break;

		// either device-list or unit-list (depending on if pContext->m_kFile.device was set)
		case FS_DEVLIST:
		{
			pContext->m_kFile.unit = 0; // we use this for enumeration
			return 0;
		}
		break;
		default: break;
	}

	pContext->m_eType = FS_INVALID;
	return -1;
#endif
}

int FileSystem_ReadFile( FSContext* pContext, char* pBuffer, int iSize )
{
#ifdef LINUX
	if( pContext->m_iFile < 0 )
		return -1;

	return read(pContext->m_iFile,pBuffer,iSize);
#else

	switch( pContext->m_eType )
	{
		case FS_IODEVICE:
		{
			if( !pContext->m_kFile.device || !(pContext->m_kFile.mode & O_RDONLY) )
				break;

			// read data from I/O device

			return pContext->m_kFile.device->ops->read( &(pContext->m_kFile), pBuffer, iSize );
		}
		break;

		default:
		break;
	}

	return -1;
#endif
}

int FileSystem_WriteFile( FSContext* pContext, const char* pBuffer, int iSize )
{
#ifdef LINUX
	if( pContext->m_iFile < 0 )
		return -1;

	return write(pContext->m_iFile,pBuffer,iSize);
#else

	switch( pContext->m_eType )
	{
		case FS_IODEVICE:
		{
			if( !pContext->m_kFile.device || !(pContext->m_kFile.mode & O_WRONLY) )
				break;

			// write data to device

			return pContext->m_kFile.device->ops->write( &(pContext->m_kFile), (char*)pBuffer, iSize );
		}
		break;

		default:
		break;
	}

	return -1;
#endif
}

int FileSystem_ReadDir( FSContext* pContext, FSFileInfo* pInfo )
{
#ifdef LINUX
	struct dirent* ent;
	struct tm* t;
	struct stat s;

	memset( pInfo, 0, sizeof(FSFileInfo) );
	pInfo->m_TS.m_iYear = 1970;
	pInfo->m_TS.m_iMonth = 1;
	pInfo->m_TS.m_iDay = 1;

	if( NULL == pContext->m_pDir )
		return -1;

	ent = readdir(pContext->m_pDir);

	if( !ent )
		return -1;

	strcpy(pInfo->m_Name,ent->d_name);

	// get file info

	strcpy( buffer, pContext->m_List );
	strcat( buffer, pInfo->m_Name );

	if( stat( buffer, &s ) < 0 )
		return -1;

	pInfo->m_eType = S_ISDIR(s.st_mode) ? FT_DIRECTORY : S_ISLNK(s.st_mode) ? FT_LINK : FT_FILE;
	pInfo->m_iSize = s.st_size;

	t = localtime(&s.st_mtime);

	pInfo->m_TS.m_iYear = t->tm_year+1900;
	pInfo->m_TS.m_iMonth = t->tm_mon+1;
	pInfo->m_TS.m_iDay = t->tm_mday;

	pInfo->m_TS.m_iHour = t->tm_hour;
	pInfo->m_TS.m_iMinute = t->tm_min;

	pInfo->m_iProtection = s.st_mode&(S_IRWXU|S_IRWXG|S_IRWXO);

	return 0;
#else
	// setup some default values for fileinfo-structure
	memset( pInfo, 0, sizeof(FSFileInfo) );
	pInfo->m_TS.m_iYear = 1970;
	pInfo->m_TS.m_iMonth = 1;
	pInfo->m_TS.m_iDay = 1;

	switch( pContext->m_eType )
	{
		case FS_IODEVICE:
		{
			if( !pContext->m_kFile.device || !(pContext->m_kFile.mode & O_DIROPEN) )
				break;

			if( (pContext->m_kFile.device->type & 0xf0000000) != IOP_DT_FSEXT )
			{
				// legacy device

				io_dirent_t ent;

				if( pContext->m_kFile.device->ops->dread( &(pContext->m_kFile), &ent ) > 0 )
				{
					strcpy(pInfo->m_Name,ent.name);

					pInfo->m_iSize = ent.stat.size;

					// fix for mc copy protected attribute, mode 47 is a directory
					pInfo->m_eType = FIO_SO_ISDIR(ent.stat.mode) || (47 == ent.stat.mode) ? FT_DIRECTORY : FIO_SO_ISLNK(ent.stat.mode) ? FT_LINK : FT_FILE;

					pInfo->m_TS.m_iYear = ent.stat.mtime[6]+1792;
					pInfo->m_TS.m_iMonth = ent.stat.mtime[5];
					pInfo->m_TS.m_iDay = ent.stat.mtime[4];

					pInfo->m_TS.m_iHour = ent.stat.mtime[3];
					pInfo->m_TS.m_iMinute = ent.stat.mtime[2];

					pInfo->m_iProtection = ent.stat.mode & (FIO_SO_IROTH|FIO_SO_IWOTH|FIO_SO_IXOTH);
					pInfo->m_iProtection = pInfo->m_iProtection|(pInfo->m_iProtection << 3)|(pInfo->m_iProtection << 6);

					// fix for mass file/directory protection attributes
					if( (ent.stat.mode == 16) || (ent.stat.mode == 32) )
						pInfo->m_iProtection = 511;

					return 0;					
				}
			}
			else
			{
				// new device

				iox_dirent_t ent;

				if( pContext->m_kFile.device->ops->dread( &(pContext->m_kFile), &ent ) > 0 )
				{
					// hide hdl and other special ps2 partitions from view
					while( !strcmp(pContext->m_kFile.device->name, "hdd") && 
						(!strncmp(ent.name, "PP.HDL.", 7) || (!strncmp(ent.name, "__", 2) && strcmp(ent.name, "__boot"))) )
					{
						// move to next entry and check if it's the last
						if( pContext->m_kFile.device->ops->dread( &(pContext->m_kFile), &ent ) == 0 )
							break;
					}

					// check last entry
					if( !strcmp(pContext->m_kFile.device->name, "hdd") && 
						(!strncmp(ent.name, "PP.HDL.", 7) || (!strncmp(ent.name, "__", 2) && strcmp(ent.name, "__boot"))) )
						break;

					strcpy(pInfo->m_Name,ent.name);

					pInfo->m_iSize = ent.stat.size;
					pInfo->m_eType = FIO_S_ISDIR(ent.stat.mode) ? FT_DIRECTORY : FIO_S_ISLNK(ent.stat.mode) ? FT_LINK : FT_FILE;

					pInfo->m_TS.m_iYear = ent.stat.mtime[6]+1792;
					pInfo->m_TS.m_iMonth = ent.stat.mtime[5];
					pInfo->m_TS.m_iDay = ent.stat.mtime[4];

					pInfo->m_TS.m_iHour = ent.stat.mtime[3];
					pInfo->m_TS.m_iMinute = ent.stat.mtime[2];

					pInfo->m_iProtection = ent.stat.mode & (FIO_S_IRWXU|FIO_S_IRWXG|FIO_S_IRWXO);

					return 0;					
				}
			}
		}
		break;

		case FS_DEVLIST:
		{
			if( pContext->m_kFile.device )
			{
				// evaluating units below a device

				while( pContext->m_kFile.unit < 16 ) // find a better value, and make a define
				{
					iox_stat_t stat;
					int ret;
					int unit = pContext->m_kFile.unit;

					memset(&stat,0,sizeof(stat));

					// fix for hdd and mass, forces subdirectory "0"
					if( !strcmp(pContext->m_kFile.device->name, "hdd") || !strcmp(pContext->m_kFile.device->name, "mass") )
					{
						itoa(pInfo->m_Name,unit);
						pInfo->m_iSize = 0;
						pInfo->m_eType = FT_DIRECTORY;
						pContext->m_kFile.unit = 16; // stops the looping
						return 0;
					}

					// fix for multiple mc directories, when both slots contain PS2 memory cards
					if( !strcmp(pContext->m_kFile.device->name, "mc") && (pContext->m_kFile.unit > 1) )
						break;

					// get status from root directory of device
					ret = pContext->m_kFile.device->ops->getstat( &(pContext->m_kFile), "/", (io_stat_t*)&stat );

					// dummy devices does not set mode properly, so we can filter them out easily
					if( (ret >= 0) && !stat.mode  )
						ret = -1;

					// increase to next unit
					pContext->m_kFile.unit++;

					// currently we stop evaluating devices if one was not found (so if mc 1 exists and not mc 0, it will not show)
					if( ret < 0 )
						return -1;

					itoa(pInfo->m_Name,unit);
					pInfo->m_iSize = 0;
					pInfo->m_eType = FT_DIRECTORY;
					return 0;
				}
			}
			else
			{
				// evaluating devices

				smod_mod_info_t* pkModule;
				iop_device_t** ppkDevices;
				int num_devices;
				int dev_offset;

				// get module

				num_devices = FS_IOMANX_DEVICES;
				dev_offset = DEVINFO_IOMANX_OFFSET;
				if( NULL == (pkModule = FileSystem_GetModule(IOPMGR_IOMANX_IDENT)) )
				{
					dev_offset = DEVINFO_IOMAN_OFFSET;
					num_devices = FS_IOMAN_DEVICES;
					if( NULL == (pkModule = FileSystem_GetModule(IOPMGR_IOMAN_IDENT)) )
						return -1;
				}

				// scan filesystem devices

				ppkDevices = DEVINFOARRAY(pkModule,dev_offset);
				while( pContext->m_kFile.unit < num_devices )
				{
					int unit = pContext->m_kFile.unit;
					pContext->m_kFile.unit++;

					if( strcmp(ppkDevices[unit]->name, "hdd") && 
						strcmp(ppkDevices[unit]->name, "mass") && 
							strcmp(ppkDevices[unit]->name, "mc") && 
								strcmp(ppkDevices[unit]->name, "pfs") )
						continue;

					if( !ppkDevices[unit] )
						continue;

					if( !(ppkDevices[unit]->type & IOP_DT_FS) )
						continue;

					strcpy(pInfo->m_Name,ppkDevices[unit]->name);
					pInfo->m_iSize = 0;
					pInfo->m_eType = FT_DIRECTORY;

					return 0;
				}
			}				
		}
		break;

		default:
			return -1;
	}

	return -1;
#endif
}


int FileSystem_DeleteFile( FSContext* pContext, const char* pFile )
{
	FileSystem_BuildPath( buffer, pContext->m_Path, pFile );

#ifdef LINUX
	return -1;
#else

	if( NULL == (pFile = FileSystem_ClassifyPath( pContext, buffer )) )
		return -1;

	switch( pContext->m_eType )
	{
		case FS_IODEVICE:
		{
			if( !pContext->m_kFile.device )
				break;

			return pContext->m_kFile.device->ops->remove( (&pContext->m_kFile), pFile );
		}
		break;

		default: return -1;
	}

	return -1;
#endif
}

int FileSystem_CreateDir( FSContext* pContext, const char* pDir )
{
	int fileMode = 0;
	FileSystem_BuildPath( buffer, pContext->m_Path, pDir );

#ifdef LINUX
	return -1;
#else
	if( NULL == (pDir = FileSystem_ClassifyPath( pContext, buffer )) )
		return -1;

	switch( pContext->m_eType )
	{
		case FS_IODEVICE:
		{
			if( !pContext->m_kFile.device )
				break;

			// check if hdd, then set permissions to 511
			if( !strcmp(pContext->m_kFile.device->name, "pfs") )
				fileMode = 511;

			return pContext->m_kFile.device->ops->mkdir( &(pContext->m_kFile), pDir, fileMode );
		}
		break;

		default: return -1;
	}

	return -1;
#endif
}

int FileSystem_DeleteDir( FSContext* pContext, const char* pDir )
{
	FileSystem_BuildPath( buffer, pContext->m_Path, pDir );

#ifdef LINUX
	return -1;
#else
	if( NULL == (pDir = FileSystem_ClassifyPath( pContext, buffer )) )
		return -1;

	switch( pContext->m_eType )
	{
		case FS_IODEVICE:
		{
			if( !pContext->m_kFile.device )
				break;
			return pContext->m_kFile.device->ops->rmdir( &(pContext->m_kFile), pDir );
		}
		break;

		default: return -1;
	}

	return -1;
#endif
}

int FileSystem_Rename( FSContext* pContext, const char* oldName, const char* newName )
{
	FileSystem_BuildPath( buffer, pContext->m_Path, newName );

#ifdef LINUX
	return -1;
#else
	if( NULL == (newName = FileSystem_ClassifyPath( pContext, buffer )) )
		return -1;

	switch( pContext->m_eType )
	{
		case FS_IODEVICE:
		{
			// rename only available to pfs
			if( !pContext->m_kFile.device || strcmp(pContext->m_kFile.device->name, "pfs") )
				break;
			return pContext->m_kFile.device->ops->rename( &(pContext->m_kFile), oldName, newName );
		}
		break;

		default: return -1;
	}

	return -1;
#endif
}


void FileSystem_Close( FSContext* pContext )
{
#ifdef LINUX
	if( pContext->m_iFile >= 0 )
	{
		close( pContext->m_iFile );
		pContext->m_iFile = -1;
	}

	if( NULL != pContext->m_pDir )
	{
		closedir( pContext->m_pDir );
		pContext->m_pDir = NULL;
	}
#else
	switch( pContext->m_eType )
	{
		case FS_IODEVICE:
		{
			if( !pContext->m_kFile.device )
				break;

			if( pContext->m_kFile.mode & O_DIROPEN )
				pContext->m_kFile.device->ops->dclose( &pContext->m_kFile );
			else
				pContext->m_kFile.device->ops->close( &pContext->m_kFile );
		}
		break;

		default: break;
	}

	pContext->m_eType = FS_INVALID;
	memset( &(pContext->m_kFile), 0, sizeof(pContext->m_kFile) );
#endif
}

void FileSystem_BuildPath( char* pResult, const char* pOriginal, const char* pAdd )
{
	pResult[0] = '\0';

	// absolute path?
	if(pAdd[0] == '/')
	{
		pOriginal = pAdd;
		pAdd = NULL;
	}

#ifdef LINUX
	strcat(pResult,"./");
#endif

	if( pOriginal )
		strcat(pResult,pOriginal);

	if( pAdd )
	{
		if( (pResult[strlen(pResult)-1] != '/') && (pResult[strlen(pResult)-1] != ':') )
			strcat( pResult, "/" );

		strcat(pResult,pAdd);
	}
}

int FileSystem_ChangeDir( FSContext* pContext, const char* pPath )
{
	strcpy(buffer,pPath);

	if( pPath[0] == '/' )
	{
		strcpy(pContext->m_Path,pPath);
		if( (pContext->m_Path[strlen(pContext->m_Path)-1] != '/') )
			strcat(pContext->m_Path,"/");
	}
	else
	{
		char* entry = strtok(buffer,"/");

		while(entry && strlen(entry)>0)
		{
			if(!strcmp(entry,".."))
			{
				char* t = strrchr(pContext->m_Path,'/');
				while( --t >= pContext->m_Path)
				{
					*(t+1) = 0;

					if( *t == '/')
						break;
				}
			}
			else
			{
				strcat( pContext->m_Path, entry );
				strcat( pContext->m_Path, "/" );
			}

			entry = strtok(NULL,"/");
		}
	}

	return 0;
}

int FileSystem_GetFileSize( FSContext* pContext, const char* pFile )
{
#ifdef LINUX
	return -1;
#else

	FileSystem_Close(pContext);
	FileSystem_BuildPath( buffer, pContext->m_Path, pFile );

	if( NULL == (pFile = FileSystem_ClassifyPath( pContext, buffer )) )
		return -1;

	switch( pContext->m_eType )
	{
		case FS_IODEVICE:
		{
			iox_stat_t stat;

			memset(&stat,0,sizeof(stat));

			// break early if hdd, otherwise get status results in "ps2fs: Warning: NULL buffer returned"
			if( !strcmp(pContext->m_kFile.device->name, "pfs") )
				break;

			// get status from root directory of device
			if( pContext->m_kFile.device->ops->getstat( &(pContext->m_kFile), pFile, (io_stat_t*)&stat ) < 0 )
				break;

			// dummy devices does not set mode properly, so we can filter them out easily
			if( !stat.mode )
				break;

			if( (pContext->m_kFile.device->type & 0xf0000000) != IOP_DT_FSEXT )
			{
				// legacy device

				if( !FIO_SO_ISREG(stat.mode) )
					break;

				pContext->m_kFile.device = NULL;
				pContext->m_eType = FS_INVALID;
				return stat.size;
			}
			else
			{
				// newstyle

				if( !FIO_S_ISREG(stat.mode) )
					break;

				pContext->m_kFile.device = NULL;
				pContext->m_eType = FS_INVALID;
				return stat.size;
			}
		}
		break;

		default:
		break;
	}

	pContext->m_kFile.device = NULL;
	pContext->m_eType = FS_INVALID;

	return -1;
#endif
}

#ifndef LINUX
const char* FileSystem_ClassifyPath( FSContext* pContext, const char* pPath )
{
	char* entry;
	char* t;

	// make sure the I/O has been closed before

	FileSystem_Close(pContext);

	// must start as a valid path

	if( pPath[0] != '/' )
		return NULL;

	// begin parsing

	strcpy(buffer,pPath);
	entry = strtok(buffer+1,"/");

	// begin parsing

	pContext->m_eType = FS_DEVLIST;

	// is this a pure device list? then just return a pointer thats != NULL
	if(!entry || !strlen(entry))
		return pPath;

	// attempt to find device
	if( NULL == (pContext->m_kFile.device = FileSystem_ScanDevice(IOPMGR_IOMANX_IDENT,FS_IOMANX_DEVICES,entry)) )
	{
		if( NULL == (pContext->m_kFile.device = FileSystem_ScanDevice(IOPMGR_IOMAN_IDENT,FS_IOMAN_DEVICES,entry)) )
			return NULL;
	}

	// extract unit number if present
	entry = strtok(NULL,"/");

	// no entry present? then we do a unit listing of the current device
	if(!entry || !strlen(entry))
		return pPath;

	t = entry;
	while(*t)
	{
		// enforcing unit nubering
		if(!isnum(*t))
			return NULL;

		t++;
	}

	pContext->m_kFile.unit = strtol(entry, NULL, 0);

	// extract local path

	pContext->m_eType = FS_IODEVICE;
	entry = strtok(NULL,"");
	strcpy(buffer,"/");

	if(entry)
		strcat(buffer,entry);	// even if buffer was passed in as an argument, this will yield a valid result

	return buffer;
}

smod_mod_info_t* FileSystem_GetModule( const char* pDevice )
{
	smod_mod_info_t* pkModule = NULL;

	// scan module list

	pkModule = (smod_mod_info_t *)0x800;
	while( NULL != pkModule )
	{
		if (!strcmp(pkModule->name, pDevice))
			break;

		pkModule = pkModule->next;
	}

	return pkModule;
}

iop_device_t* FileSystem_ScanDevice( const char* pDevice, int iNumDevices, const char* pPath )
{
	smod_mod_info_t* pkModule;
  iop_device_t **ppkDevices;
	int i;
	int offset;

	// get module

	if( NULL == (pkModule = FileSystem_GetModule(pDevice)) )
		return NULL;

	// determine offset
	
	if( !strcmp(IOPMGR_IOMANX_IDENT,pkModule->name) )
		offset = DEVINFO_IOMANX_OFFSET;
	else if( !strcmp(IOPMGR_IOMAN_IDENT,pkModule->name) )
		offset = DEVINFO_IOMAN_OFFSET;
	else
		return NULL; // unknown device, we cannot determine the offset here...

	// get device info array
	ppkDevices = DEVINFOARRAY(pkModule,offset);

	// scan array
	for( i = 0; i < iNumDevices; i++ )
	{
		if( (NULL != ppkDevices[i]) ) // note, last compare is to avoid a bug when mounting partitions right now that I have to track down
		{
			if( (ppkDevices[i]->type & IOP_DT_FS))
			{
				if( !strncmp(pPath,ppkDevices[i]->name,strlen(ppkDevices[i]->name)) )
					return ppkDevices[i];
			}
		}
	}

	return NULL;
}

//! Mount device
int FileSystem_MountDevice( FSContext* pContext, const char* mount_point, const char* mount_file )
{
	if( !mount_point || !mount_file )
		return -1;

	if( NULL == (mount_point = FileSystem_ClassifyPath(pContext,mount_point)))
		return -1;

	if( (FS_IODEVICE != pContext->m_eType) || (NULL == pContext->m_kFile.device) )
		return -1;

	if( (pContext->m_kFile.device->type & 0xf0000000) != IOP_DT_FSEXT )
	{
#ifdef DEBUG
		printf("ps2ftpd: device fs is not extended.\n");
#endif
		pContext->m_eType = FS_INVALID;
		return -1;
	}

	if( pContext->m_kFile.device->ops->mount( &(pContext->m_kFile), mount_point, mount_file, 0, NULL, 0 ) < 0 )
	{
#ifdef DEBUG
		printf("ps2ftpd: device-mount failed.\n");
#endif
		pContext->m_eType = FS_INVALID;
		return -1;
	}

	DelayThread(100);

	pContext->m_eType = FS_INVALID;
	return 0;
}

//! Unmount device
int FileSystem_UnmountDevice( FSContext* pContext, const char* mount_point )
{
	if(!mount_point)
		return -1;

	if( NULL == (mount_point = FileSystem_ClassifyPath(pContext,mount_point)))
		return -1;

	if( (FS_IODEVICE != pContext->m_eType) || (NULL == pContext->m_kFile.device) )
		return -1;

	if( (pContext->m_kFile.device->type & 0xf0000000) != IOP_DT_FSEXT )
	{
#ifdef DEBUG
		printf("ps2ftpd: device fs is not extended.\n");
#endif
		pContext->m_eType = FS_INVALID;
		return -1;
	}

	if( pContext->m_kFile.device->ops->umount( &(pContext->m_kFile), mount_point ) < 0 )
	{
#ifdef DEBUG
		printf("ps2ftpd: device-unmount failed.\n");
#endif
		pContext->m_eType = FS_INVALID;
		return -1;
	}

	pContext->m_eType = FS_INVALID;
	return 0;
}

//! Synchronize device
int FileSystem_SyncDevice( FSContext* pContext, const char* devname )
{
	if(!devname)
		return -1;

	if( NULL == (devname = FileSystem_ClassifyPath(pContext,devname)))
		return -1;

	if( (FS_IODEVICE != pContext->m_eType) || (NULL == pContext->m_kFile.device) )
		return -1;

	if( (pContext->m_kFile.device->type & 0xf0000000) != IOP_DT_FSEXT )
	{
#ifdef DEBUG
		printf("ps2ftpd: device fs is not extended.\n");
#endif
		pContext->m_eType = FS_INVALID;
		return -1;
	}

	if( pContext->m_kFile.device->ops->sync( &(pContext->m_kFile), devname, 0 ) < 0 )
	{
#ifdef DEBUG
		printf("ps2ftpd: device-sync failed.\n");
#endif
		pContext->m_eType = FS_INVALID;
		return -1;
	}

	pContext->m_eType = FS_INVALID;
	return 0;
}

#endif
