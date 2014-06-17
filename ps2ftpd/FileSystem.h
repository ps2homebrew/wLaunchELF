#ifndef __FILESYSTEM_H__
#define __FILESYSTEM_H__

/*
 * FileSystem.h - FTP server filesystem declarations
 *
 * Copyright (C) 2004 Jesper Svennevid
 *
 * Licensed under the AFL v2.0. See the file LICENSE included with this
 * distribution for licensing terms.
 *
 */

#define FS_IOMAN_DEVICES 16
#define FS_IOMANX_DEVICES 32

#ifdef LINUX
#include <dirent.h>
#else

#include <iomanX.h>
#include <iopmgr.h>

typedef enum
{
	FS_INVALID,
	FS_DEVLIST,
	FS_IODEVICE, // ioman or iomanX
} FSType;

#endif

typedef enum
{
	FM_READ,
	FM_WRITE,
} FileMode;

typedef struct FSContext
{
	char m_Path[256];

#ifndef LINUX
	FSType m_eType;
	iop_file_t m_kFile;
#else
	int m_iFile;
	DIR* m_pDir;
	char m_List[256];
#endif
} FSContext;

typedef enum
{
	FT_FILE,
	FT_DIRECTORY,
	FT_LINK,
} FileType;

typedef struct FSFileInfo
{
	struct
	{
		int			m_iYear;
		int			m_iMonth;
		int			m_iDay;
		int			m_iHour;
		int			m_iMinute;
	} m_TS; // timestamp

	FileType	m_eType;	
	int				m_iProtection; // 3 bits protection per section: UrwxGrwxOrwx

	int				m_iSize;
	char			m_Name[256];
} FSFileInfo;

//! Initialize context for use
void FileSystem_Create( FSContext* pContext );

//! Cleanup context
void FileSystem_Destroy( FSContext* pContext );

//! Open file for readin or writing
int FileSystem_OpenFile( FSContext* pContext, const char* pFile, FileMode eMode, int iContinue );

//! Open directory for listing
int FileSystem_OpenDir( FSContext* pContext, const char* pDir );

//! Read data from open file
int FileSystem_ReadFile( FSContext* pContext, char* pBuffer, int iSize );

//! Write data to open file
int FileSystem_WriteFile( FSContext* pContext, const char* pBuffer, int iSize );

//! Read directory
int FileSystem_ReadDir( FSContext* pContext, FSFileInfo* pInfo );

//! Close file or directory
void FileSystem_Close( FSContext* pContext );

//! Convert path from unified name to PS2 specific path (no verification of existance)
void FileSystem_BuildPath( char* pResult, const char* pOriginal, const char* pAdd );

//! Change directory
int FileSystem_ChangeDir( FSContext* pContext, const char* pPath );

//! Delete file
int FileSystem_DeleteFile( FSContext* pContext, const char* pFile );

//! Create new directory
int FileSystem_CreateDir( FSContext* pContext, const char* pDir );

//! Get file-size
int FileSystem_GetFileSize( FSContext* pContext, const char* pFile );

//! Delete directory
int FileSystem_DeleteDir( FSContext* pContext, const char* pDir );

//! Rename file or directory
int FileSystem_Rename( FSContext* pContext, const char* oldName, const char* newName );

#ifndef LINUX
//! Determine device needed for accessing device & fill context with info
const char* FileSystem_ClassifyPath( FSContext* pContext, const char* pPath );

//! Get a pointer to device handler
smod_mod_info_t* FileSystem_GetModule( const char* pDevice );

//! Scan devices & return ops structure if found
iop_device_t* FileSystem_ScanDevice( const char* pDevice, int iNumDevices, const char* pPath );

//! Mount device
int FileSystem_MountDevice( FSContext* pContext, const char* mount_point, const char* mount_file );

//! Unmount device
int FileSystem_UnmountDevice( FSContext* pContext, const char* mount_point );

//! Sync device
int FileSystem_SyncDevice( FSContext* pContext, const char* devname );
#endif

#endif
