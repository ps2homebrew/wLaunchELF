//--------------------------------------------------------------
//File name:   iopmod_name.h           Revision Date: 2005.06.14
//Created by:  Ronald Andersson        Creation Date: 2005.06.14
//Purpose:     Define names of PS2 IOP modules and libraries
//--------------------------------------------------------------
#ifndef _IOPMOD_NAME_
#define _IOPMOD_NAME_

#ifdef __cplusplus
extern "C" {
#endif

//#define IOPMOD_NAME_        ""
#define	IOPMOD_NAME_ATAD      "atad"
#define	IOPMOD_NAME_CDVDMAN   "cdvd_driver"
#define	IOPMOD_NAME_DEV9      "dev9"
#define	IOPMOD_NAME_FAKEHOST  "fakehost"
#define	IOPMOD_NAME_FILEIO    "FILEIO_service"
#define	IOPMOD_NAME_FILEXIO   "IOX/File_Manager_Rpc"
#define	IOPMOD_NAME_FREESD    "freesd"
#define	IOPMOD_NAME_HDD       "hdd"
#define	IOPMOD_NAME_ILINK     "iLINK_Driver"
#define IOPMOD_NAME_IOMAN     "IO/File_Manager"
#define IOPMOD_NAME_IOMANX    "IOX/File_Manager"
#define IOPMOD_NAME_IOPMGR    "IOP_Manager"
#define	IOPMOD_NAME_LIBSD     "Sound_Device_Library"
#define IOPMOD_NAME_LOADFILE  "LoadModuleByEE"
#define	IOPMOD_NAME_MCMAN     "mcman"
#define	IOPMOD_NAME_MCSERV    "mcserv"
#define IOPMOD_NAME_MODLOAD   "Moldule_File_loader" //NB: spelling error is in ROM!
#define	IOPMOD_NAME_PADMAN    "padman"
#define	IOPMOD_NAME_PFS       "pfs"
#define	IOPMOD_NAME_POWEROFF  "Poweroff_Handler"
#define	IOPMOD_NAME_PS2NETFS  "PS2_TcpFileDriver"
#define	IOPMOD_NAME_PS2ATAD   "atad_driver"
#define IOPMOD_NAME_PS2DEV9   "dev9_driver"
#define	IOPMOD_NAME_PS2FS     "pfs_driver"
#define	IOPMOD_NAME_PS2HDD    "hdd_driver"
#define	IOPMOD_NAME_PS2IP     "TCP/IP Stack"
#define	IOPMOD_NAME_SIO2MAN   "sio2man"
#define	IOPMOD_NAME_SIOR      "sioremote_driver"
#define	IOPMOD_NAME_SMBMAN    "smbman"
#define	IOPMOD_NAME_SMAP      "INET_SMAP_driver"
#define IOPMOD_NAME_THREADMAN "Multi_Thread_Manager"
#define	IOPMOD_NAME_USBD      "USB_driver"
#define	IOPMOD_NAME_VMC_FS    "vmc_fs"
#define IOPMOD_NAME_XLOADFILE "LoadModuleByEE" //NB: same name as for LOADFILE
#define IOPMOD_NAME_XMCMAN    "mcman_cex"
#define IOPMOD_NAME_XMCSERV   "mcserv" //NB: same name as for MCSERV

//NB: some modules exist with conflicting names, and even no name at all...
//This includes USBD, PS2IP, PS2SMAP, PS2FTPD, PS2HOST, and probably others
//Some alternate names are defined below
//#define IOPMOD_NAM2_        ""
#define	IOPMOD_NAM2_USBD      "usbd"

//Some modules also register as runtime libraries, with library names that
//are independent of the module name. Libraries for same purpose may often
//have the same library name even though they have different module names,
//while some libraries don't even have any module name...
//#define IOPLIB_NAME_        ""
#define IOPLIB_NAME_IOPMGR    "iopmgr"
#define IOPLIB_NAME_MODLOAD   "modload"

#ifdef __cplusplus
}
#endif

#endif // _IOPMOD_NAME_
//--------------------------------------------------------------
//End of file: iopmod_name.h
//--------------------------------------------------------------
