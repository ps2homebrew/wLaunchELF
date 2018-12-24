#include "vmc.h"


//  Driver fonctions

#define MODNAME "vmc_fs"
IRX_ID(MODNAME, 1, 2);


// Static variables.
static iop_device_t s_Vmc_Driver;
static iop_device_ops_t s_Vmc_Functions;

// Global variables.
struct global_vmc g_Vmc_Image[2];
int g_Vmc_Remove_Flag;
int g_Vmc_Initialized;

//----------------------------------------------------------------------------
// Initialize vmc driver
//----------------------------------------------------------------------------
int Vmc_Initialize(iop_device_t *driver)
{

	if (g_Vmc_Initialized)
		return VMCFS_ERR_NO;

	g_Vmc_Initialized = TRUE;

	//  init the global file descriptor to something negative ( ubergeek like 42, and polo like 35 so - 4235 )
	g_Vmc_Image[0].fd = -4235;
	g_Vmc_Image[1].fd = -4235;

	DEBUGPRINT(1, "vmc_fs: Filesystem Driver Initialized\n");

	return VMCFS_ERR_NO;
}


//----------------------------------------------------------------------------
// Deinitialize vmc driver
//----------------------------------------------------------------------------
int Vmc_Deinitialize(iop_device_t *driver)
{

	int i = 0;

	if (!g_Vmc_Initialized)
		return VMCFS_ERR_INITIALIZED;

	g_Vmc_Initialized = FALSE;

	for (i = 0; i < 2; i++) {

		if (g_Vmc_Image[i].fd >= 0) {

			close(g_Vmc_Image[i].fd);
		}
	}

	DEBUGPRINT(1, "vmc_fs: Filesystem Driver Deinitialized\n");

	return VMCFS_ERR_NO;
}

// Entry point of the driver.
int _start(int argc, char **argv)
{

	printf("vmc_fs: Created by ubergeek42\n");
	printf("vmc_fs: ... Improved by Polo35.\n");
	printf("vmc_fs: Version 1.2\n");

	g_Vmc_Initialized = FALSE;
	g_Vmc_Remove_Flag = FALSE;

	s_Vmc_Driver.type = (IOP_DT_FS | IOP_DT_FSEXT);
	s_Vmc_Driver.version = 1;
	s_Vmc_Driver.name = "vmc";  //  the name used to access this device ( eg: vmc0:  /  )
	s_Vmc_Driver.desc = "Virtual Memory Card s_Vmc_Driver";
	s_Vmc_Driver.ops = &s_Vmc_Functions;

	//  Typecasting to ( void* ) eliminates a bunch of warnings.
	s_Vmc_Functions.init = Vmc_Initialize;
	s_Vmc_Functions.deinit = Vmc_Deinitialize;
	s_Vmc_Functions.format = Vmc_Format;
	s_Vmc_Functions.open = Vmc_Open;
	s_Vmc_Functions.close = Vmc_Close;
	s_Vmc_Functions.read = Vmc_Read;
	s_Vmc_Functions.write = Vmc_Write;
	s_Vmc_Functions.lseek = Vmc_Lseek;
	s_Vmc_Functions.ioctl = Vmc_Ioctl;
	s_Vmc_Functions.remove = Vmc_Remove;
	s_Vmc_Functions.mkdir = Vmc_Mkdir;
	s_Vmc_Functions.rmdir = Vmc_Rmdir;
	s_Vmc_Functions.dopen = Vmc_Dopen;
	s_Vmc_Functions.dclose = Vmc_Dclose;
	s_Vmc_Functions.dread = Vmc_Dread;
	s_Vmc_Functions.getstat = Vmc_Getstat;
	s_Vmc_Functions.chstat = Vmc_Chstat;
	/* Extended ops start here. */
	s_Vmc_Functions.rename = Vmc_Rename;
	s_Vmc_Functions.chdir = Vmc_Chdir;
	s_Vmc_Functions.sync = Vmc_Sync;
	s_Vmc_Functions.mount = Vmc_Mount;
	s_Vmc_Functions.umount = Vmc_Umount;
	s_Vmc_Functions.lseek64 = Vmc_Lseek64;
	s_Vmc_Functions.devctl = Vmc_Devctl;
	s_Vmc_Functions.symlink = Vmc_Symlink;
	s_Vmc_Functions.readlink = Vmc_Readlink;
	s_Vmc_Functions.ioctl2 = Vmc_Ioctl2;


	//  Add the Driver using the ioman export
	DelDrv(s_Vmc_Driver.name);
	AddDrv(&s_Vmc_Driver);

	//  And we are done!
	DEBUGPRINT(1, "vmc_fs: Filesystem Driver %s added!\n", s_Vmc_Driver.name);

	return 0;
}
