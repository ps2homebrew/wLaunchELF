//--------------------------------------------------------------
//File name:    loader.c
//--------------------------------------------------------------
//dlanor: This subprogram has been modified to minimize the code
//dlanor: size of the resident loader portion. Some of the parts
//dlanor: that were moved into the main program include loading
//dlanor: of all IRXs and mounting pfs0: for ELFs on hdd.
//dlanor: Another change was to skip threading in favor of ExecPS2
/*==================================================================
==											==
==	Copyright(c)2004  Adam Metcalf(gamblore_@hotmail.com)		==
==	Copyright(c)2004  Thomas Hawcroft(t0mb0la@yahoo.com)		==
==	This file is subject to terms and conditions shown in the	==
==	file LICENSE which should be kept in the top folder of	==
==	this distribution.							==
==											==
==	Portions of this code taken from PS2Link:				==
==				pkoLoadElf						==
==				wipeUserMemory					==
==				(C) 2003 Tord Lindstrom (pukko@home.se)	==
==				(C) 2003 adresd (adresd_ps2dev@yahoo.com)	==
==	Portions of this code taken from Independence MC exploit	==
==				tLoadElf						==
==				LoadAndRunHDDElf					==
==				(C) 2003 Marcus Brown <mrbrown@0xd6.org>	==
==											==
==================================================================*/
#include "tamtypes.h"
#include "debug.h"
#include "kernel.h"
#include "sifrpc.h"
#include "loadfile.h"
#include "string.h"
#include "iopheap.h"
#include "errno.h"
//--------------------------------------------------------------
//#define DEBUG
#ifdef DEBUG
#define dbgprintf(args...) scr_printf(args)
#define dbginit_scr() init_scr()
#else
#define dbgprintf(args...) \
    do {                   \
    } while (0)
#define dbginit_scr() \
    do {              \
    } while (0)
#endif

static char HDDpath[256];
static char partition[128];

//--------------------------------------------------------------
//End of data declarations
//--------------------------------------------------------------
//Start of function code:
//--------------------------------------------------------------
// Clear user memory
// PS2Link (C) 2003 Tord Lindstrom (pukko@home.se)
//         (C) 2003 adresd (adresd_ps2dev@yahoo.com)
//--------------------------------------------------------------
static void wipeUserMem(void)
{
    int i;
    for (i = 0x100000; i < 0x2000000; i += 64) {
        asm volatile(
            "\tsq $0, 0(%0) \n"
            "\tsq $0, 16(%0) \n"
            "\tsq $0, 32(%0) \n"
            "\tsq $0, 48(%0) \n" ::"r"(i));
    }
}
//--------------------------------------------------------------
//End of func:  void wipeUserMem(void)
//--------------------------------------------------------------
// C standard strrchr func.. returns pointer to the last occurance of a
// character in a string, or NULL if not found
// PS2Link (C) 2003 Tord Lindstrom (pukko@home.se)
//         (C) 2003 adresd (adresd_ps2dev@yahoo.com)
//--------------------------------------------------------------
char *strrchr(const char *sp, int i)
{
    const char *last = NULL;
    char c = i;

    while (*sp) {
        if (*sp == c) {
            last = sp;
        }
        sp++;
    }

    if (*sp == c) {
        last = sp;
    }

    return (char *)last;
}
//--------------------------------------------------------------
//End of func:  char *strrchr(const char *sp, int i)
//--------------------------------------------------------------
// *** MAIN ***
//--------------------------------------------------------------
int main(int argc, char *argv[])
{
    t_ExecData elfdata;
    char s[256], fakepart[128], *ptr, *args[1];
    int ret;

    // Initialize
    SifInitRpc(0);
    dbginit_scr();
    wipeUserMem();
    dbgprintf("Welcome to Loader of LaunchELF v3.50\nPlease wait...loading.\n");

    strcpy(s, argv[0]);
    dbgprintf("argv[0] = %s\n", s);
    if (argc == 1) {  // should be two params passed by menu
        SifExitRpc();
        return 0;
               // leave this here for adding mc0, host or other
               // to be added in future
    }
    if (argc == 2)  // if call came from hddmenu.elf
    {               // arg1=path to ELF, arg2=partition to mount
        strcpy(partition, argv[1]);
        dbgprintf("argv[1] = %s\n", partition);
        strcpy(HDDpath, s);
    }
    dbgprintf("Loading %s\n", HDDpath);
    FlushCache(0);
    ret = SifLoadElf(HDDpath, &elfdata);
    dbgprintf("SifLoadElf returned %i\n", ret);
    if(ret == 0) {
        if (!strncmp(HDDpath, "pfs0", 4)) {
            strcpy(fakepart, HDDpath);
            ptr = strrchr(fakepart, '/');
            if (ptr == NULL)
                strcpy(fakepart, "pfs0:");
            else {
                ptr++;
                *ptr = '\0';
            }
            ptr = strrchr(s, '/');
            if (ptr == NULL)
                ptr = strrchr(s, ':');
            if (ptr != NULL) {
                ptr++;
                strcpy(HDDpath, "host:");
                strcat(HDDpath, ptr);
            }
        }

        args[0] = HDDpath;

        SifExitRpc();

        FlushCache(0);
        FlushCache(2);

        ExecPS2((void *)elfdata.epc, (void *)elfdata.gp, 1, args);
    } else {
        dbgprintf("failed\n"
        	  "Could not execute file %s\n", HDDpath);

        SifExitRpc();
    }

    return 0;
}
//--------------------------------------------------------------
//End of func:  int main(int argc, char *argv[])
//--------------------------------------------------------------
//End of file:  loader.c
//--------------------------------------------------------------
