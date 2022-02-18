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
extern char *itoa(char *in, int val);

// these offsets depend on the layout of both ioman & iomanX ... ps2netfs does
// something similar, so if it breaks, we all go down.
#define DEVINFO_IOMAN_OFFSET  0x0c
#define DEVINFO_IOMANX_OFFSET 0

#define DEVINFOARRAY(d, ofs) ((iop_device_t **)((d)->text_start + (d)->text_size + (d)->data_size + (ofs)))

#define DEVICE_UNITS 10

// buffer used for concating filenames internally
static char buffer[512];

void FileSystem_Create(FSContext *pContext)
{
    strcpy(pContext->m_Path, "/");

#ifndef LINUX
    pContext->m_eType = FS_INVALID;
    memset(&(pContext->m_kFile), 0, sizeof(pContext->m_kFile));
#else
    pContext->m_iFile = -1;
    pContext->m_pDir = NULL;
#endif
}

void FileSystem_Destroy(FSContext *pContext)
{
    FileSystem_Close(pContext);
}

int FileSystem_OpenFile(FSContext *pContext, const char *pFile, FileMode eMode, int iContinue)
{
    int flags;
    int fileMode = 0;
    int iOpened = 0;

    FileSystem_Close(pContext);
    FileSystem_BuildPath(buffer, pContext->m_Path, pFile);

    switch (eMode) {
        case FM_READ:
            flags = O_RDONLY;
            break;
        case FM_WRITE:
            flags = O_CREAT | O_TRUNC | O_WRONLY;
            break;

        default:
            return -1;
    }

#ifdef LINUX
    if ((pContext->m_iFile = open(buffer, flags, S_IRWXU | S_IRGRP)) < 0)
        return -1;

    return 0;
#else
    if (NULL == (pFile = FileSystem_ClassifyPath(pContext, buffer)))
        return -1;

    switch (pContext->m_eType) {
        case FS_IODEVICE: {
            if (iContinue) {
                if (flags & O_WRONLY) {
                    pContext->m_kFile.mode = O_WRONLY;
                    if (pContext->m_kFile.device->ops->open(&(pContext->m_kFile), pFile, pContext->m_kFile.mode, 0) >= 0)
                        iOpened = 1;
                } else {
                    pContext->m_kFile.mode = O_RDONLY;
                    if (pContext->m_kFile.device->ops->open(&(pContext->m_kFile), pFile, pContext->m_kFile.mode, 0) >= 0)
                        iOpened = 1;
                }

                // seek to position if we successfully opened the file

                if (iOpened) {
                    if (iContinue < 0) {
                        if (pContext->m_kFile.device->ops->lseek(&(pContext->m_kFile), 0, SEEK_END) >= 0)
                            return 0;
                    } else {
                        if (pContext->m_kFile.device->ops->lseek(&(pContext->m_kFile), iContinue, SEEK_SET) >= 0)
                            return 0;
                    }

                    // could not seek, close file and open normally
                    pContext->m_kFile.device->ops->close(&(pContext->m_kFile));
                }
            }

            pContext->m_kFile.mode = flags;

            // check if hdd, then set permissions to 511
            if (!strcmp(pContext->m_kFile.device->name, "pfs"))
                fileMode = 511;

            if (pContext->m_kFile.device->ops->open(&(pContext->m_kFile), pFile, flags, fileMode) >= 0)
                return 0;

        } break;

        default:
            return -1;
    }

    pContext->m_eType = FS_INVALID;
    return -1;
#endif
}

int FileSystem_OpenDir(FSContext *pContext, const char *pDir)
{
    FileSystem_Close(pContext);
    FileSystem_BuildPath(buffer, pContext->m_Path, pDir);

#ifdef LINUX

    strcpy(pContext->m_List, buffer);

    // unsafe, sure
    pContext->m_pDir = opendir(pContext->m_List);
    return pContext->m_pDir ? 0 : -1;

#else

    if (NULL == (pDir = FileSystem_ClassifyPath(pContext, buffer)))
        return -1;

    switch (pContext->m_eType) {
        // directory listing from I/O device
        case FS_IODEVICE: {
            if (!pContext->m_kFile.device)
                break;

            // attempt to open device directory

            pContext->m_kFile.mode = O_DIROPEN;
            if (pContext->m_kFile.device->ops->dopen(&(pContext->m_kFile), pDir) >= 0)
                return 0;
        } break;

        // either device-list or unit-list (depending on if pContext->m_kFile.device was set)
        case FS_DEVLIST: {
            pContext->m_kFile.unit = 0;  // we use this for enumeration
            return 0;
        } break;
        default:
            break;
    }

    pContext->m_eType = FS_INVALID;
    return -1;
#endif
}

int FileSystem_ReadFile(FSContext *pContext, char *pBuffer, int iSize)
{
#ifdef LINUX
    if (pContext->m_iFile < 0)
        return -1;

    return read(pContext->m_iFile, pBuffer, iSize);
#else

    switch (pContext->m_eType) {
        case FS_IODEVICE: {
            if (!pContext->m_kFile.device || !(pContext->m_kFile.mode & O_RDONLY))
                break;

            // read data from I/O device

            return pContext->m_kFile.device->ops->read(&(pContext->m_kFile), pBuffer, iSize);
        } break;

        default:
            break;
    }

    return -1;
#endif
}

int FileSystem_WriteFile(FSContext *pContext, const char *pBuffer, int iSize)
{
#ifdef LINUX
    if (pContext->m_iFile < 0)
        return -1;

    return write(pContext->m_iFile, pBuffer, iSize);
#else

    switch (pContext->m_eType) {
        case FS_IODEVICE: {
            if (!pContext->m_kFile.device || !(pContext->m_kFile.mode & O_WRONLY))
                break;

            // write data to device

            return pContext->m_kFile.device->ops->write(&(pContext->m_kFile), (char *)pBuffer, iSize);
        } break;

        default:
            break;
    }

    return -1;
#endif
}

int FileSystem_ReadDir(FSContext *pContext, FSFileInfo *pInfo)
{
#ifdef LINUX
    struct dirent *ent;
    struct tm *t;
    struct stat s;

    memset(pInfo, 0, sizeof(FSFileInfo));
    pInfo->m_TS.m_iYear = 1970;
    pInfo->m_TS.m_iMonth = 1;
    pInfo->m_TS.m_iDay = 1;

    if (NULL == pContext->m_pDir)
        return -1;

    ent = readdir(pContext->m_pDir);

    if (!ent)
        return -1;

    strcpy(pInfo->m_Name, ent->d_name);

    // get file info

    strcpy(buffer, pContext->m_List);
    strcat(buffer, pInfo->m_Name);

    if (stat(buffer, &s) < 0)
        return -1;

    pInfo->m_eType = S_ISDIR(s.st_mode) ? FT_DIRECTORY : S_ISLNK(s.st_mode) ? FT_LINK :
                                                                              FT_FILE;
    pInfo->m_iSize = s.st_size;

    t = localtime(&s.st_mtime);

    pInfo->m_TS.m_iYear = t->tm_year + 1900;
    pInfo->m_TS.m_iMonth = t->tm_mon + 1;
    pInfo->m_TS.m_iDay = t->tm_mday;

    pInfo->m_TS.m_iHour = t->tm_hour;
    pInfo->m_TS.m_iMinute = t->tm_min;

    pInfo->m_iProtection = s.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO);

    return 0;
#else
    ps2time tm;
    getPs2Time(&tm);

    // set default values off the clock of the ps2
    memset(pInfo, 0, sizeof(FSFileInfo));
    pInfo->m_TS.m_iYear = tm.year;
    pInfo->m_TS.m_iMonth = tm.month;
    pInfo->m_TS.m_iDay = tm.day;
    pInfo->m_TS.m_iHour = tm.hour;
    pInfo->m_TS.m_iMinute = tm.min;
    pInfo->m_TS.m_iSecond = tm.sec;

    switch (pContext->m_eType) {
        case FS_IODEVICE: {
            if (!pContext->m_kFile.device || !(pContext->m_kFile.mode & O_DIROPEN))
                break;

            if ((pContext->m_kFile.device->type & 0xf0000000) != IOP_DT_FSEXT) {
                // legacy device

                io_dirent_t ent;

                if (pContext->m_kFile.device->ops->dread(&(pContext->m_kFile), (iox_dirent_t *)&ent) > 0) {
                    strcpy(pInfo->m_Name, ent.name);

                    // fix for mc copy protected attribute, mode 0x002F is a directory
                    if (FIO_SO_ISDIR(ent.stat.mode) == FT_DIRECTORY || ent.stat.mode == 0x002F) {
                        pInfo->m_iSize = 0;
                        pInfo->m_eType = FT_DIRECTORY;
                    } else {
                        pInfo->m_iSize = ((u64)ent.stat.hisize << 32) | (u32)ent.stat.size;
                        pInfo->m_eType = FIO_SO_ISLNK(ent.stat.mode) ? FT_LINK : FT_FILE;
                    }

                    if (ent.stat.mtime[6] != 0) {
                        pInfo->m_TS.m_iYear = (ent.stat.mtime[7] << 8) + ent.stat.mtime[6];
                        pInfo->m_TS.m_iMonth = ent.stat.mtime[5];
                        pInfo->m_TS.m_iDay = ent.stat.mtime[4];

                        pInfo->m_TS.m_iHour = ent.stat.mtime[3];
                        pInfo->m_TS.m_iMinute = ent.stat.mtime[2];
                        pInfo->m_TS.m_iSecond = ent.stat.mtime[1];
                    }

                    pInfo->m_iDaysBetween = getDaysBetween(pInfo->m_TS.m_iMonth, pInfo->m_TS.m_iDay, pInfo->m_TS.m_iYear, tm.month, tm.day, tm.year);

                    pInfo->m_iProtection = ent.stat.mode & (FIO_SO_IROTH | FIO_SO_IWOTH | FIO_SO_IXOTH);
                    pInfo->m_iProtection = pInfo->m_iProtection | (pInfo->m_iProtection << 3) | (pInfo->m_iProtection << 6);

                    // fix for mass file/directory protection attributes (note: only needed with older USB drivers)
                    if ((ent.stat.mode == 0x0010) || (ent.stat.mode == 0x0020))
                        pInfo->m_iProtection = 0x1FF;

                    /* debug info
                    printf( "Name:          \t%s\n", ent.name );
                    printf( "Size:          \t%li\n", (u32)ent.stat.size );
                    printf( "Mode:          \t0x%04X\n", ent.stat.mode );
                    printf( "Created:       \t%02i/%02i/%i \t", ent.stat.ctime[5], ent.stat.ctime[4], (ent.stat.ctime[7] << 8) + ent.stat.ctime[6] );
                    printf( "%02i:%02i:%02i\n", ent.stat.ctime[3], ent.stat.ctime[2], ent.stat.ctime[1] );
                    printf( "Modified:      \t%02i/%02i/%i \t", ent.stat.mtime[5], ent.stat.mtime[4], (ent.stat.mtime[7] << 8) + ent.stat.mtime[6] );
                    printf( "%02i:%02i:%02i\n", ent.stat.mtime[3], ent.stat.mtime[2], ent.stat.mtime[1] );
                    if( !strcmp(pContext->m_kFile.device->name, "mass") )
                        printf( "Accessed:      \t%02i/%02i/%i\n", ent.stat.atime[5], ent.stat.atime[4], (ent.stat.atime[7] << 8) + ent.stat.atime[6] );
                    else if( strcmp(pContext->m_kFile.device->name, "mc") )
                    {
                        printf( "Accessed:      \t%02i/%02i/%i \t", ent.stat.atime[5], ent.stat.atime[4], (ent.stat.atime[7] << 8) + ent.stat.atime[6] );
                        printf( "%02i:%02i:%02i\n", ent.stat.atime[3], ent.stat.atime[2], ent.stat.atime[1] );
                    }
                    printf( "System time:   \t%02i/%02i/%i \t", tm.month, tm.day, tm.year );
                    printf( "%02i:%02i:%02i  (%i days between)\n\n", tm.hour, tm.min, tm.sec, pInfo->m_iDaysBetween );
                    */

                    return 0;
                }
            } else {
                // new device

                iox_dirent_t ent;

                if (pContext->m_kFile.device->ops->dread(&(pContext->m_kFile), &ent) > 0) {
                    // hdd partition filter: skip over certain ps2 hdd partitions
                    while (!strcmp(pContext->m_kFile.device->name, "hdd") &&
                           ((ent.stat.attr != 0x0000 || ent.stat.mode != 0x0100) ||
                            (!strncmp(ent.name, "PP.", 3) && !strcmp(ent.name + (strlen(ent.name)) - 4, ".PCB")))) {
                        // move to next entry and check if it's the last
                        if (pContext->m_kFile.device->ops->dread(&(pContext->m_kFile), &ent) == 0) {
                            // check last entry
                            if (!strcmp(pContext->m_kFile.device->name, "hdd") &&
                                ((ent.stat.attr != 0x0000 || ent.stat.mode != 0x0100) ||
                                 (!strncmp(ent.name, "PP.", 3) && !strcmp(ent.name + (strlen(ent.name)) - 4, ".PCB"))))
                                return -1;

                            break;
                        }
                    }

                    strcpy(pInfo->m_Name, ent.name);

                    // auto-mounting partitions: changed hdd partitions to directories
                    if (FIO_S_ISDIR(ent.stat.mode) == FT_DIRECTORY || ent.stat.mode == 0x0100) {
                        pInfo->m_iSize = 0;
                        pInfo->m_eType = FT_DIRECTORY;
                    } else {
                        pInfo->m_iSize = ((u64)ent.stat.hisize << 32) | (u32)ent.stat.size;
                        pInfo->m_eType = FIO_S_ISLNK(ent.stat.mode) ? FT_LINK : FT_FILE;
                    }

                    pInfo->m_TS.m_iYear = (ent.stat.mtime[7] << 8) + ent.stat.mtime[6];
                    pInfo->m_TS.m_iMonth = ent.stat.mtime[5];
                    pInfo->m_TS.m_iDay = ent.stat.mtime[4];

                    pInfo->m_TS.m_iHour = ent.stat.mtime[3];
                    pInfo->m_TS.m_iMinute = ent.stat.mtime[2];
                    pInfo->m_TS.m_iSecond = ent.stat.mtime[1];

                    pInfo->m_iDaysBetween = getDaysBetween(pInfo->m_TS.m_iMonth, pInfo->m_TS.m_iDay, pInfo->m_TS.m_iYear, tm.month, tm.day, tm.year);

                    pInfo->m_iProtection = ent.stat.mode & (FIO_S_IRWXU | FIO_S_IRWXG | FIO_S_IRWXO);

                    /* debug info
                    printf( "Name:          \t%s\n", ent.name );
                    printf( "Size:          \t%li\n", (u32)ent.stat.size );
                    printf( "Mode:          \t0x%04X\n", ent.stat.mode );
                    printf( "Attr:          \t0x%04X\n", ent.stat.attr );
                    printf( "Created:       \t%02i/%02i/%i \t", ent.stat.ctime[5], ent.stat.ctime[4], (ent.stat.ctime[7] << 8) + ent.stat.ctime[6] );
                    printf( "%02i:%02i:%02i\n", ent.stat.ctime[3], ent.stat.ctime[2], ent.stat.ctime[1] );
                    printf( "Modified:      \t%02i/%02i/%i \t", ent.stat.mtime[5], ent.stat.mtime[4], (ent.stat.mtime[7] << 8) + ent.stat.mtime[6] );
                    printf( "%02i:%02i:%02i\n", ent.stat.mtime[3], ent.stat.mtime[2], ent.stat.mtime[1] );
                    printf( "Accessed:      \t%02i/%02i/%i \t", ent.stat.atime[5], ent.stat.atime[4], (ent.stat.atime[7] << 8) + ent.stat.atime[6] );
                    printf( "%02i:%02i:%02i\n", ent.stat.atime[3], ent.stat.atime[2], ent.stat.atime[1] );
                    printf( "System time:   \t%02i/%02i/%i \t", tm.month, tm.day, tm.year );
                    printf( "%02i:%02i:%02i  (%i days between)\n\n", tm.hour, tm.min, tm.sec, pInfo->m_iDaysBetween );
                    */

                    return 0;
                }
            }
        } break;

        case FS_DEVLIST: {
            if (pContext->m_kFile.device) {
                // evaluating units below a device

                while (pContext->m_kFile.unit < DEVICE_UNITS) {
                    iox_stat_t stat;
                    int ret;
                    int unit = pContext->m_kFile.unit;

                    memset(&stat, 0, sizeof(stat));

                    // fix for hdd, forces subdirectory "0"
                    if (!strcmp(pContext->m_kFile.device->name, "hdd")) {
                        itoa(pInfo->m_Name, unit);
                        pInfo->m_iSize = 0;
                        pInfo->m_eType = FT_DIRECTORY;
                        pContext->m_kFile.unit = 10;  // stop evaluating units below device
                        return 0;
                    }

                    // get status from root directory of device
                    ret = pContext->m_kFile.device->ops->getstat(&(pContext->m_kFile), "/", &stat);

                    // fix for mass, forces subdirecory "0" (note: only needed with older USB drivers)
                    if ((unit == 0) && (ret != -19) && !strcmp(pContext->m_kFile.device->name, "mass")) {
                        itoa(pInfo->m_Name, unit);
                        pInfo->m_iSize = 0;
                        pInfo->m_eType = FT_DIRECTORY;
                        pContext->m_kFile.unit++;
                        return 0;
                    }

                    // dummy devices does not set mode properly, so we can filter them out easily
                    if ((ret >= 0) && !stat.mode)
                        ret = -1;

                    // determine status from stat and mode for both PS2 and PS1 memory cards
                    if (!strcmp(pContext->m_kFile.device->name, "mc")) {
                        if ((stat.mode == 0x0027) || (stat.mode == 0x0000 && (ret == -4 || ret == -2)))
                            ret = 0;
                        // fix for multiple mc directories, when both slots contain memory cards
                        if (pContext->m_kFile.unit > 0)
                            pContext->m_kFile.unit = 10;  // stop evaluating units below device
                    }

                    // stop looking for mounted hdd partitions after four
                    if (!strcmp(pContext->m_kFile.device->name, "pfs") && (pContext->m_kFile.unit > 2))
                        pContext->m_kFile.unit = 10;  // stop evaluating units below device

                    // increase to next unit
                    pContext->m_kFile.unit++;

                    // move to next device unit, if one was not found
                    if (ret < 0)
                        continue;

                    itoa(pInfo->m_Name, unit);
                    pInfo->m_iSize = 0;
                    pInfo->m_eType = FT_DIRECTORY;
                    return 0;
                }
            } else {
                // evaluating devices

                ModuleInfo_t *pkModule;
                iop_device_t **ppkDevices;
                int num_devices;
                int dev_offset;

                // get module

                num_devices = FS_IOMANX_DEVICES;
                dev_offset = DEVINFO_IOMANX_OFFSET;
                if (NULL == (pkModule = FileSystem_GetModule(IOPMGR_IOMANX_IDENT))) {
                    dev_offset = DEVINFO_IOMAN_OFFSET;
                    num_devices = FS_IOMAN_DEVICES;
                    if (NULL == (pkModule = FileSystem_GetModule(IOPMGR_IOMAN_IDENT)))
                        return -1;
                }

                // scan filesystem devices

                ppkDevices = DEVINFOARRAY(pkModule, dev_offset);
                while (pContext->m_kFile.unit < num_devices) {
                    int unit = pContext->m_kFile.unit;
                    pContext->m_kFile.unit++;

                    if (strcmp(ppkDevices[unit]->name, "hdd") &&
                        strcmp(ppkDevices[unit]->name, "mass") &&
                        strcmp(ppkDevices[unit]->name, "mc") &&
                        strcmp(ppkDevices[unit]->name, "pfs"))
                        continue;

                    if (!ppkDevices[unit])
                        continue;

                    if (!(ppkDevices[unit]->type & (IOP_DT_FS | IOP_DT_BLOCK)))
                        continue;

                    strcpy(pInfo->m_Name, ppkDevices[unit]->name);
                    pInfo->m_iSize = 0;
                    pInfo->m_eType = FT_DIRECTORY;

                    return 0;
                }
            }
        } break;

        default:
            return -1;
    }

    return -1;
#endif
}


int FileSystem_DeleteFile(FSContext *pContext, const char *pFile)
{
    FileSystem_BuildPath(buffer, pContext->m_Path, pFile);

#ifdef LINUX
    return -1;
#else

    if (NULL == (pFile = FileSystem_ClassifyPath(pContext, buffer)))
        return -1;

    switch (pContext->m_eType) {
        case FS_IODEVICE: {
            if (!pContext->m_kFile.device)
                break;

            return pContext->m_kFile.device->ops->remove((&pContext->m_kFile), pFile);
        } break;

        default:
            return -1;
    }

    return -1;
#endif
}

int FileSystem_CreateDir(FSContext *pContext, const char *pDir)
{
    int fileMode = 0;
    FileSystem_BuildPath(buffer, pContext->m_Path, pDir);

#ifdef LINUX
    return -1;
#else
    if (NULL == (pDir = FileSystem_ClassifyPath(pContext, buffer)))
        return -1;

    switch (pContext->m_eType) {
        case FS_IODEVICE: {
            if (!pContext->m_kFile.device)
                break;

            // check if hdd, then set permissions to 511
            if (!strcmp(pContext->m_kFile.device->name, "pfs"))
                fileMode = 511;

            return pContext->m_kFile.device->ops->mkdir(&(pContext->m_kFile), pDir, fileMode);
        } break;

        default:
            return -1;
    }

    return -1;
#endif
}

int FileSystem_DeleteDir(FSContext *pContext, const char *pDir)
{
    FileSystem_BuildPath(buffer, pContext->m_Path, pDir);

#ifdef LINUX
    return -1;
#else
    if (NULL == (pDir = FileSystem_ClassifyPath(pContext, buffer)))
        return -1;

    switch (pContext->m_eType) {
        case FS_IODEVICE: {
            if (!pContext->m_kFile.device)
                break;
            return pContext->m_kFile.device->ops->rmdir(&(pContext->m_kFile), pDir);
        } break;

        default:
            return -1;
    }

    return -1;
#endif
}

int FileSystem_Rename(FSContext *pContext, const char *oldName, const char *newName)
{
    FileSystem_BuildPath(buffer, pContext->m_Path, newName);

#ifdef LINUX
    return -1;
#else
    if (NULL == (newName = FileSystem_ClassifyPath(pContext, buffer)))
        return -1;

    switch (pContext->m_eType) {
        case FS_IODEVICE: {
            // rename only available to FSEXT devices.
            if (!pContext->m_kFile.device || ((pContext->m_kFile.device->type & 0xf0000000) != IOP_DT_FSEXT))
                break;
            return pContext->m_kFile.device->ops->rename(&(pContext->m_kFile), oldName, newName);
        } break;

        default:
            return -1;
    }

    return -1;
#endif
}


void FileSystem_Close(FSContext *pContext)
{
#ifdef LINUX
    if (pContext->m_iFile >= 0) {
        close(pContext->m_iFile);
        pContext->m_iFile = -1;
    }

    if (NULL != pContext->m_pDir) {
        closedir(pContext->m_pDir);
        pContext->m_pDir = NULL;
    }
#else
    switch (pContext->m_eType) {
        case FS_IODEVICE: {
            if (!pContext->m_kFile.device)
                break;

            if (pContext->m_kFile.mode & O_DIROPEN)
                pContext->m_kFile.device->ops->dclose(&pContext->m_kFile);
            else
                pContext->m_kFile.device->ops->close(&pContext->m_kFile);
        } break;

        default:
            break;
    }

    pContext->m_eType = FS_INVALID;
    memset(&(pContext->m_kFile), 0, sizeof(pContext->m_kFile));
#endif
}

void FileSystem_BuildPath(char *pResult, const char *pOriginal, const char *pAdd)
{
    pResult[0] = '\0';

    // absolute path?
    if (pAdd[0] == '/') {
        pOriginal = pAdd;
        pAdd = NULL;
    }

#ifdef LINUX
    strcat(pResult, "./");
#endif

    if (pOriginal)
        strcat(pResult, pOriginal);

    if (pAdd) {
        if ((pResult[strlen(pResult) - 1] != '/') && (pResult[strlen(pResult) - 1] != ':'))
            strcat(pResult, "/");

        strcat(pResult, pAdd);
    }
}

int FileSystem_ChangeDir(FSContext *pContext, const char *pPath)
{
    strcpy(buffer, pPath);

    if (pPath[0] == '/') {
        strcpy(pContext->m_Path, pPath);
        if ((pContext->m_Path[strlen(pContext->m_Path) - 1] != '/'))
            strcat(pContext->m_Path, "/");
    } else {
        char *entry = strtok(buffer, "/");

        while (entry && strlen(entry) > 0) {
            if (!strcmp(entry, "..")) {
                char *t = strrchr(pContext->m_Path, '/');
                while (--t >= pContext->m_Path) {
                    *(t + 1) = 0;

                    if (*t == '/')
                        break;
                }
            } else {
                strcat(pContext->m_Path, entry);
                strcat(pContext->m_Path, "/");
            }

            entry = strtok(NULL, "/");
        }
    }

    // auto-mounting partitions: issue unmount command before listing hdd partitions
    if (!strcmp(pContext->m_Path, "/hdd/0/"))
        FileSystem_UnmountDevice(pContext, "/pfs/0/");

    // auto-mounting partitions: mount partition, then change target
    else if (!strncmp(pContext->m_Path, "/hdd/0/", 7)) {
        int i;
        char mount[512] = "hdd:";
        char target[512] = "/pfs/0/";

        for (i = 7; pContext->m_Path[i] != '/'; i++)
            mount[i - 3] = pContext->m_Path[i];

        // issue unmount command
        FileSystem_UnmountDevice(pContext, target);

        // issue mount command to the selected partition, then change target
        if (FileSystem_MountDevice(pContext, target, mount) == 0) {
            strcat(target, pContext->m_Path + i + 1);
            strcpy(pContext->m_Path, target);
        }

        // failed to mount partition, change target back to hdd/0
        else
            strcpy(pContext->m_Path, "/hdd/0/");
    }
    return 0;
}

int FileSystem_GetFileSize(FSContext *pContext, const char *pFile)
{
#ifdef LINUX
    return -1;
#else

    FileSystem_Close(pContext);
    FileSystem_BuildPath(buffer, pContext->m_Path, pFile);

    if (NULL == (pFile = FileSystem_ClassifyPath(pContext, buffer)))
        return -1;

    switch (pContext->m_eType) {
        case FS_IODEVICE: {
            iox_stat_t stat;

            memset(&stat, 0, sizeof(stat));

            // get status from root directory of device
            if (pContext->m_kFile.device->ops->getstat(&(pContext->m_kFile), pFile, &stat) < 0)
                break;

            // dummy devices does not set mode properly, so we can filter them out easily
            if (!stat.mode)
                break;

            if ((pContext->m_kFile.device->type & 0xf0000000) != IOP_DT_FSEXT) {
                // legacy device

                if (!FIO_SO_ISREG(stat.mode))
                    break;

                pContext->m_kFile.device = NULL;
                pContext->m_eType = FS_INVALID;
                return stat.size;
            } else {
                // newstyle

                if (!FIO_S_ISREG(stat.mode))
                    break;

                pContext->m_kFile.device = NULL;
                pContext->m_eType = FS_INVALID;
                return stat.size;
            }
        } break;

        default:
            break;
    }

    pContext->m_kFile.device = NULL;
    pContext->m_eType = FS_INVALID;

    return -1;
#endif
}

#ifndef LINUX
const char *FileSystem_ClassifyPath(FSContext *pContext, const char *pPath)
{
    char *entry;
    char *t;

    // make sure the I/O has been closed before

    FileSystem_Close(pContext);

    // must start as a valid path

    if (pPath[0] != '/')
        return NULL;

    // begin parsing

    strcpy(buffer, pPath);
    entry = strtok(buffer + 1, "/");

    // begin parsing

    pContext->m_eType = FS_DEVLIST;

    // is this a pure device list? then just return a pointer thats != NULL
    if (!entry || !strlen(entry))
        return pPath;

    // attempt to find device
    if (NULL == (pContext->m_kFile.device = FileSystem_ScanDevice(IOPMGR_IOMANX_IDENT, FS_IOMANX_DEVICES, entry))) {
        if (NULL == (pContext->m_kFile.device = FileSystem_ScanDevice(IOPMGR_IOMAN_IDENT, FS_IOMAN_DEVICES, entry)))
            return NULL;
    }

    // extract unit number if present
    entry = strtok(NULL, "/");

    // no entry present? then we do a unit listing of the current device
    if (!entry || !strlen(entry))
        return pPath;

    t = entry;
    while (*t) {
        // enforcing unit nubering
        if (!isnum(*t))
            return NULL;

        t++;
    }

    pContext->m_kFile.unit = strtol(entry, NULL, 0);

    // extract local path

    pContext->m_eType = FS_IODEVICE;
    entry = strtok(NULL, "");
    strcpy(buffer, "/");

    if (entry)
        strcat(buffer, entry);  // even if buffer was passed in as an argument, this will yield a valid result

    return buffer;
}

ModuleInfo_t *FileSystem_GetModule(const char *pDevice)
{
    ModuleInfo_t *pkModule = NULL;

    // scan module list

    pkModule = (ModuleInfo_t *)0x800;
    while (NULL != pkModule) {
        if (!strcmp((char *)pkModule->name, pDevice))
            break;

        pkModule = pkModule->next;
    }

    return pkModule;
}

iop_device_t *FileSystem_ScanDevice(const char *pDevice, int iNumDevices, const char *pPath)
{
    ModuleInfo_t *pkModule;
    iop_device_t **ppkDevices;
    int i;
    int offset;

    // get module

    if (NULL == (pkModule = FileSystem_GetModule(pDevice)))
        return NULL;

    // determine offset

    if (!strcmp(IOPMGR_IOMANX_IDENT, (char *)pkModule->name))
        offset = DEVINFO_IOMANX_OFFSET;
    else if (!strcmp(IOPMGR_IOMAN_IDENT, (char *)pkModule->name))
        offset = DEVINFO_IOMAN_OFFSET;
    else
        return NULL;  // unknown device, we cannot determine the offset here...

    // get device info array
    ppkDevices = DEVINFOARRAY(pkModule, offset);

    // scan array
    for (i = 0; i < iNumDevices; i++) {
        if ((NULL != ppkDevices[i]))  // note, last compare is to avoid a bug when mounting partitions right now that I have to track down
        {
            if ((ppkDevices[i]->type & (IOP_DT_FS | IOP_DT_BLOCK))) {
                if (!strncmp(pPath, ppkDevices[i]->name, strlen(ppkDevices[i]->name)))
                    return ppkDevices[i];
            }
        }
    }

    return NULL;
}

//! Mount device
int FileSystem_MountDevice(FSContext *pContext, const char *mount_point, const char *mount_file)
{
    if (!mount_point || !mount_file)
        return -1;

    if (NULL == (mount_point = FileSystem_ClassifyPath(pContext, mount_point)))
        return -1;

    if ((FS_IODEVICE != pContext->m_eType) || (NULL == pContext->m_kFile.device))
        return -1;

    if ((pContext->m_kFile.device->type & 0xf0000000) != IOP_DT_FSEXT) {
#ifdef DEBUG
        printf("ps2ftpd: device fs is not extended.\n");
#endif
        pContext->m_eType = FS_INVALID;
        return -1;
    }

    if (pContext->m_kFile.device->ops->mount(&(pContext->m_kFile), mount_point, mount_file, 0, NULL, 0) < 0) {
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
int FileSystem_UnmountDevice(FSContext *pContext, const char *mount_point)
{
    if (!mount_point)
        return -1;

    if (NULL == (mount_point = FileSystem_ClassifyPath(pContext, mount_point)))
        return -1;

    if ((FS_IODEVICE != pContext->m_eType) || (NULL == pContext->m_kFile.device))
        return -1;

    if ((pContext->m_kFile.device->type & 0xf0000000) != IOP_DT_FSEXT) {
#ifdef DEBUG
        printf("ps2ftpd: device fs is not extended.\n");
#endif
        pContext->m_eType = FS_INVALID;
        return -1;
    }

    if (pContext->m_kFile.device->ops->umount(&(pContext->m_kFile), mount_point) < 0) {
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
int FileSystem_SyncDevice(FSContext *pContext, const char *devname)
{
    if (!devname)
        return -1;

    if (NULL == (devname = FileSystem_ClassifyPath(pContext, devname)))
        return -1;

    if ((FS_IODEVICE != pContext->m_eType) || (NULL == pContext->m_kFile.device))
        return -1;

    if ((pContext->m_kFile.device->type & 0xf0000000) != IOP_DT_FSEXT) {
#ifdef DEBUG
        printf("ps2ftpd: device fs is not extended.\n");
#endif
        pContext->m_eType = FS_INVALID;
        return -1;
    }

    if (pContext->m_kFile.device->ops->sync(&(pContext->m_kFile), devname, 0) < 0) {
#ifdef DEBUG
        printf("ps2ftpd: device-sync failed.\n");
#endif
        pContext->m_eType = FS_INVALID;
        return -1;
    }

    pContext->m_eType = FS_INVALID;
    return 0;
}

int getPs2Time(ps2time *tm)
{
    sceCdCLOCK cdtime;
    s32 tmp;
    // used if can not get time...
    ps2time timeBuf = {0, 0, 0, 0, 1, 1, 1970};

    if (sceCdReadClock(&cdtime) != 0 && cdtime.stat == 0) {
        tmp = cdtime.second >> 4;
        timeBuf.sec = (int)(((tmp << 2) + tmp) << 1) + (cdtime.second & 0x0F);
        tmp = cdtime.minute >> 4;
        timeBuf.min = (((tmp << 2) + tmp) << 1) + (cdtime.minute & 0x0F);
        tmp = cdtime.hour >> 4;
        timeBuf.hour = (((tmp << 2) + tmp) << 1) + (cdtime.hour & 0x0F);
        tmp = cdtime.day >> 4;
        timeBuf.day = (((tmp << 2) + tmp) << 1) + (cdtime.day & 0x0F);
        tmp = (cdtime.month >> 4) & 0x7F;
        timeBuf.month = (((tmp << 2) + tmp) << 1) + (cdtime.month & 0x0F);
        tmp = cdtime.year >> 4;
        timeBuf.year = (((tmp << 2) + tmp) << 1) + (cdtime.year & 0xF) + 2000;
    }
    memcpy(tm, &timeBuf, sizeof(ps2time));
    return 0;
}

int getDaysBetween(int month1, int day1, int year1, int month2, int day2, int year2)
{
    int num_days = 0;
    int reverse;
    int i;

    if ((reverse = (year1 > year2)) || ((year1 == year2) && ((month1 > month2) || ((month1 == month2) && (day1 > day2))))) {
        i = year1;
        year1 = year2;
        year2 = i;
        i = month1;
        month1 = month2;
        month2 = i;
        i = day1;
        day1 = day2;
        day2 = i;
    }

    // do a gross total of the span of years, then adjust
    for (i = year1; i <= year2; i++)
        num_days += (365 + ((i) % 4 == 0 && ((i) % 100 != 0 || (i) % 400 == 0)));

    // adjust first year
    for (i = 1; i < month1; i++)
        num_days -= ((i) == 2 ? 28 + ((year1) % 4 == 0 && ((year1) % 100 != 0 || (year1) % 400 == 0)) : 30 + ((i) + ((i) >= 8)) % 2);

    // adjust last year
    for (i = month2; i <= 12; i++)
        num_days -= ((i) == 2 ? 28 + ((year2) % 4 == 0 && ((year2) % 100 != 0 || (year2) % 400 == 0)) : 30 + ((i) + ((i) >= 8)) % 2);

    num_days += day2 - day1;

    return (reverse ? -num_days : num_days);
}

#endif
