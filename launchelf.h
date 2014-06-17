#ifndef LAUNCHELF_H
#define LAUNCHELF_H

#include <stdio.h>
#include <tamtypes.h>
#include <sifcmd.h>
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <string.h>
#include <malloc.h>
#include <libhdd.h>
#include <libmc.h>
#include <libpad.h>
#include <fileio.h>
#include <sys/stat.h>
#include <iopheap.h>
#include <errno.h>
#include <fileXio_rpc.h>
#include <iopcontrol.h>
#include <stdarg.h>
#include <sbv_patches.h>
#include <slib.h>
#include <smem.h>
#include <smod.h>
#include <sys/fcntl.h>
#include <debug.h>
#include <ito.h>
#include <cdvd_rpc.h>
#include "cd.h"
#include "mass_rpc.h"
#include "iopmod_name.h"

// 垂直スキャンレート
#define SCANRATE (ITO_VMODE_AUTO==ITO_VMODE_NTSC ? 60:50)

enum
{
	SCREEN_WIDTH = 512,
	SCREEN_HEIGHT = 432,
	SCREEN_MARGIN = 12,
	FONT_WIDTH = 8,
	FONT_HEIGHT = 16,
	LINE_THICKNESS = 2,
	
	MAX_NAME = 256,
	MAX_PATH = 1025,
	MAX_ENTRY = 2048,
	MAX_ROWS = 21,
	MAX_PARTITIONS=100
};

typedef struct
{
	char dirElf[15][MAX_PATH];
	int timeout;
	int filename;
	uint64 color[4];
	int screen_x;
	int screen_y;
	int discControl;
	int interlace;
	int resetIOP;
	int numCNF;
	int swapKeys;
} SETTING;

typedef struct
{
	int ip[4];
	int nm[4];
	int gw[4];
} data_ip_struct;

extern char LaunchElfDir[MAX_PATH], LastDir[MAX_NAME];

void	load_ps2host(void);
void loadCdModules(void);
void loadUsbModules(void);
void loadHddModules(void);

/* elf.c */
int checkELFheader(const char *filename);
void RunLoaderElf(char *filename, char *);

/* draw.c */
extern itoGsEnv screen_env;
void setScrTmp(const char *msg0, const char *msg1);
void drawMsg(const char *msg);
void setupito(void);
void clrScr(uint64 color);
void drawScr(void);
void drawFrame(int x1, int y1, int x2, int y2, uint64 color);
void drawChar(unsigned char c, int x, int y, uint64 colour);
int printXY(const unsigned char *s, int x, int y, uint64 colour, int);

/* pad.c */
extern u32 new_pad;
int setupPad(void);
int readpad(void);
void waitPadReady(int port, int slot);

/* config.c */
extern SETTING *setting;
void loadConfig(char *, char *);
void config(char *, char *);

/* filer.c */
unsigned char *elisaFnt;
void getFilePath(char *out, const int cnfmode);
void	initHOST(void);
char *makeHostPath(char *dp, char*sp);


/* main.c */
int swapKeys;

#endif
