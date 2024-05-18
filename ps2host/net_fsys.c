/*********************************************************************
 * Copyright (C) 2003 Tord Lindstrom (pukko@home.se)
 * Copyright (C) 2004 adresd (adresd_ps2dev@yahoo.com)
 * This file is subject to the terms and conditions of the PS2Link License.
 * See the file LICENSE in the main directory of this distribution for more
 * details.
 */

#include <types.h>
#include <thbase.h>
#include <ioman.h>
#include <sysclib.h>
#include <stdio.h>
#include <intrman.h>
#include <loadcore.h>
#include <thsemap.h>

#include "net_fio.h"

#define IOCTL_RENAME 0xFEEDC0DE  // dlanor: Used for the Ioctl request code => Rename

#ifdef DEBUG
#define dbgprintf(args...) printf(args)
#else
#define dbgprintf(args...) \
    do {                   \
    } while (0)
#endif

static char fsname[] = "host";

//----------------------------------------------------------------------------

/* File desc struct is probably larger than this, but we only
 * need to access the word @ offset 0x0C (in which we put our identifier)
 */
struct filedesc_info
{
    int unkn0;
    int unkn4;
    int device_id;  // the X in hostX
    int own_fd;
};
// dlanor: In fact this struct is declared elsewhere as iop_file_t, with
// dlanor: well known fields. The one declared as "int own_fd;" above is
// dlanor: there declared as "void	*privdata;" but it doesn't matter, as
// dlanor: it is up to each driver how to use that field. So using it as
// dlanor: a simple "int" instead of a pointer will work fine.


//----------------------------------------------------------------------------
/* We need(?) to protect the net access, so the server doesn't get
 * confused if two processes calls a fsys func at the same time
 */
static int fsys_sema;
static int fsys_pid = 0;

//----------------------------------------------------------------------------
// dlanor: We need variables to remember a file/folder descriptor used
// dlanor: with the Ioctl Rename function, so that we can avoid passing
// dlanor: the following fioClose call to PC, where it's already closed.
// dlanor: We also need a flag to note when we must ignore an Mkdir call
// dlanor: because it is caused by the bug IOMAN.IRX has for fioRemove.
//
int lastopen_fd;      // descriptor of the most recently opened file/folder
int renamed_fd;       // descriptor of renamed file/folder awaiting closure
int remove_flag = 0;  // Set in fsysRemove, cleared by all other fsysXXXXX
int remove_result;    // Set in fsysRemove, so fsysMkdir can use it for bug

typedef void (*th_func_p)(void *);  // dlanor: added to suppress warnings

//----------------------------------------------------------------------------
static void fsysInit(iop_device_t *driver)
{
    struct _iop_thread mythread;
    int pid;

    dbgprintf("initializing %s\n", driver->name);

    remove_flag = 0;

    // Start socket server thread

    mythread.attr = 0x02000000;                  // attr
    mythread.option = 0;                         // option
    mythread.thread = (th_func_p)pko_file_serv;  // entry
    mythread.stacksize = 0x800;
    mythread.priority = 0x45;  // We really should choose prio w more effort

    pid = CreateThread(&mythread);

    if (pid > 0) {
        int i;

        if ((i = StartThread(pid, NULL)) < 0) {
            printf("StartThread failed (%d)\n", i);
        }
    } else {
        printf("CreateThread failed (%d)\n", pid);
    }

    fsys_pid = pid;
    dbgprintf("Thread id: %x\n", pid);
}
//----------------------------------------------------------------------------
static int fsysDestroy(void)
{
    remove_flag = 0;

    WaitSema(fsys_sema);
    pko_close_fsys();
    //    ExitDeleteThread(fsys_pid);
    SignalSema(fsys_sema);
    DeleteSema(fsys_sema);
    return 0;
}


//----------------------------------------------------------------------------
static int dummyFormat()
{
    remove_flag = 0;

    printf("dummy Format function called\n");
    return -5;
}
//----------------------------------------------------------------------------
static int fsysOpen(int fd, char *name, int mode)
{
    struct filedesc_info *fd_info;
    int fsys_fd;

    dbgprintf("fsysOpen..\n");
    dbgprintf("  fd: %x, name: %s, mode: %d\n\n", fd, name, mode);

    remove_flag = 0;

    fd_info = (struct filedesc_info *)fd;

    WaitSema(fsys_sema);
    fsys_fd = pko_open_file(name, mode);
    SignalSema(fsys_sema);
    fd_info->own_fd = fsys_fd;
    renamed_fd = -1;
    lastopen_fd = fsys_fd;

    return fsys_fd;
}
//----------------------------------------------------------------------------
static int fsysClose(int fd)
{
    int remote_fd = ((struct filedesc_info *)fd)->own_fd;
    int ret;

    dbgprintf("fsys_close..\n"
              "  fd: %x\n\n",
              fd);

    remove_flag = 0;

    if (remote_fd != renamed_fd) {
        WaitSema(fsys_sema);
        ret = pko_close_file(remote_fd);
        SignalSema(fsys_sema);
    } else
        ret = 0;
    renamed_fd = -1;

    return ret;
}
//----------------------------------------------------------------------------
static int fsysRead(int fd, char *buf, int size)
{
    const struct filedesc_info *fd_info;
    int ret;

    fd_info = (struct filedesc_info *)fd;

    dbgprintf("fsysRead..."
              "  fd: %x\n"
              "  bf: %x\n"
              "  sz: %d\n"
              "  ow: %d\n\n",
              fd, (int)buf, size, fd_info->own_fd);

    remove_flag = 0;

    WaitSema(fsys_sema);
    ret = pko_read_file(fd_info->own_fd, buf, size);
    SignalSema(fsys_sema);

    return ret;
}
//----------------------------------------------------------------------------
static int fsysWrite(int fd, char *buf, int size)
{
    const struct filedesc_info *fd_info;
    int ret;

    dbgprintf("fsysWrite..."
              "  fd: %x\n",
              fd);

    remove_flag = 0;

    fd_info = (struct filedesc_info *)fd;
    WaitSema(fsys_sema);
    ret = pko_write_file(fd_info->own_fd, buf, size);
    SignalSema(fsys_sema);
    return ret;
}
//----------------------------------------------------------------------------
static int fsysLseek(int fd, unsigned int offset, int whence)
{
    const struct filedesc_info *fd_info;
    int ret;

    dbgprintf("fsysLseek..\n"
              "  fd: %x\n"
              "  of: %x\n"
              "  wh: %x\n\n",
              fd, offset, whence);

    remove_flag = 0;

    fd_info = (struct filedesc_info *)fd;
    WaitSema(fsys_sema);
    ret = pko_lseek_file(fd_info->own_fd, offset, whence);
    SignalSema(fsys_sema);
    return ret;
}
//----------------------------------------------------------------------------
static int fsysIoctl(iop_file_t *file, unsigned long request, void *data)
{
    int ret;
    dbgprintf("fsysioctl..\n");
    // dbgprintf("  fd: %x\n"
    //           "  rq: %x\n"
    //           "  dp: %x\n\n", );

    remove_flag = 0;

    if (request == IOCTL_RENAME) {
        int remote_fd = ((struct filedesc_info *)file)->own_fd;

        if (lastopen_fd == remote_fd) {
            WaitSema(fsys_sema);
            ret = pko_ioctl(remote_fd, request, data);
            SignalSema(fsys_sema);
            if ((ret == 0) || (ret == -6))
                renamed_fd = remote_fd;
        } else {
            printf("Ioctl Rename function used incorrectly.\n");
            ret = -5;
        }
    } else {
        printf("Ioctl function called with invalid request code\n");
        ret = -5;
    }
    return ret;
}
//----------------------------------------------------------------------------
static int fsysRemove(iop_file_t *file, char *name)
{
    int ret;
    dbgprintf("fsysRemove..\n");
    dbgprintf("  name: %s\n\n", name);

    remove_flag = 1;

    WaitSema(fsys_sema);
    ret = pko_remove(name);
    SignalSema(fsys_sema);

    remove_result = ret;
    return ret;
}
//----------------------------------------------------------------------------
static int fsysMkdir(iop_file_t *file, char *name, int mode)
{
    int ret;
    dbgprintf("fsysMkdir..\n");
    dbgprintf("  name: '%s'\n\n", name);

    if (remove_flag == 0) {
        WaitSema(fsys_sema);
        ret = pko_mkdir(name, mode);
        SignalSema(fsys_sema);
    } else {
        remove_flag = 0;
        ret = remove_result;
    }

    return ret;
}
//----------------------------------------------------------------------------
static int fsysRmdir(iop_file_t *file, char *name)
{
    int ret;
    dbgprintf("fsysRmdir..\n");
    dbgprintf("  name: %s\n\n", name);

    remove_flag = 0;

    WaitSema(fsys_sema);
    ret = pko_rmdir(name);
    SignalSema(fsys_sema);

    return ret;
}
//----------------------------------------------------------------------------
static int fsysDopen(int fd, char *name)
{
    struct filedesc_info *fd_info;
    int fsys_fd;

    dbgprintf("fsysDopen..\n");
    dbgprintf("  fd: %x, name: %s\n\n", fd, name);

    fd_info = (struct filedesc_info *)fd;

    remove_flag = 0;

    WaitSema(fsys_sema);
    fsys_fd = pko_open_dir(name);
    SignalSema(fsys_sema);
    fd_info->own_fd = fsys_fd;
    renamed_fd = -1;
    lastopen_fd = fsys_fd;

    return fsys_fd;
}
//----------------------------------------------------------------------------
static int fsysDclose(int fd)
{
    int remote_fd = ((struct filedesc_info *)fd)->own_fd;
    int ret;

    dbgprintf("fsys_dclose..\n"
              "  fd: %x\n\n",
              fd);

    remove_flag = 0;

    if (remote_fd != renamed_fd) {
        WaitSema(fsys_sema);
        ret = pko_close_dir(remote_fd);
        SignalSema(fsys_sema);
    } else
        ret = 0;
    renamed_fd = -1;

    return ret;
}
//----------------------------------------------------------------------------
static int fsysDread(int fd, void *buf)
{
    const struct filedesc_info *fd_info;
    int ret;

    fd_info = (struct filedesc_info *)fd;

    dbgprintf("fsysDread..."
              "  fd: %x\n"
              "  bf: %x\n"
              "  ow: %d\n\n",
              fd, (int)buf, fd_info->own_fd);

    remove_flag = 0;

    WaitSema(fsys_sema);
    ret = pko_read_dir(fd_info->own_fd, buf);
    SignalSema(fsys_sema);

    return ret;
}
//----------------------------------------------------------------------------
static int dummyGetstat()
{
    remove_flag = 0;

    printf("dummy Getstat function called\n");
    return -5;
}
//----------------------------------------------------------------------------
static int dummyChstat()
{
    remove_flag = 0;

    printf("dummy Chstat function called\n");
    return -5;
}
//----------------------------------------------------------------------------
iop_device_ops_t fsys_functarray =
    {(void *)fsysInit,
     (void *)fsysDestroy,
     (void *)dummyFormat,  // init, deinit, format
     (void *)fsysOpen,
     (void *)fsysClose,
     (void *)fsysRead,  // open, close, read,
     (void *)fsysWrite,
     (void *)fsysLseek,
     (void *)fsysIoctl,  // write, lseek, ioctl
     (void *)fsysRemove,
     (void *)fsysMkdir,
     (void *)fsysRmdir,  // remove, mkdir, rmdir
     (void *)fsysDopen,
     (void *)fsysDclose,
     (void *)fsysDread,  // dopen, dclose, dread
     (void *)dummyGetstat,
     (void *)dummyChstat};  // getstat, chstat

iop_device_t fsys_driver = {fsname, 16, 1, "fsys driver",
                            &fsys_functarray};
//----------------------------------------------------------------------------
// Entry point for mounting the file system
int fsysMount(void)
{
    iop_sema_t sema_info;

    sema_info.attr = 1;
    sema_info.option = 0;
    sema_info.initial = 1;
    sema_info.max = 1;
    fsys_sema = CreateSema(&sema_info);

    DelDrv(fsname);
    AddDrv(&fsys_driver);

    return 0;
}

//----------------------------------------------------------------------------
int fsysUnmount(void)
{
    DelDrv(fsname);
    return 0;
}
//----------------------------------------------------------------------------
// End of file:  net_fsys.c
//----------------------------------------------------------------------------
