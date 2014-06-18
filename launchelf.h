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
#include <libjpg.h>

#define SCANRATE (ITO_VMODE_AUTO==ITO_VMODE_NTSC ? 60:50)

// sincro needed for modified getfilepath
#define USBD_IRX_CNF 3
// polo needed for modified getfilepath
#define SKIN_CNF 4

//#define GS_border_colour itoSetBgColor(setting->color[0]) /* Old method */
#define GS_border_colour 0 /* New method */

enum
{
	SCREEN_MARGIN = 16,
	FONT_WIDTH = 8,
	FONT_HEIGHT = 16,
	LINE_THICKNESS = 2,
	
	MAX_NAME = 256,
	MAX_PATH = 1025,
	MAX_ENTRY = 2048,
	MAX_PARTITIONS=100,
	MAX_TITLE = 40
};

typedef struct
{
	char dirElf[15][MAX_PATH];
	char usbd[MAX_PATH];
	char skin[MAX_PATH];
	char Menu_Title[MAX_TITLE+1];
	int  Menu_Frame;
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
	int HOSTwrite;
	int Brightness;
	int TV_mode;
	int Popup_Opaque;
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
#define BACKGROUND_PIC	0
#define PREVIEW_PIC			1

extern itoGsEnv screen_env;
extern int      testskin;
extern int      testsetskin;
extern int      SCREEN_WIDTH;
extern int      SCREEN_HEIGHT;
extern int      SCREEN_X;
extern int      SCREEN_Y;
extern int			Menu_start_x;
extern int			Menu_title_y;
extern int			Menu_message_y;
extern int			Frame_start_y;
extern int			Menu_start_y;
extern int			Menu_end_y;
extern int			Frame_end_y;
extern int			Menu_tooltip_y;

void setScrTmp(const char *msg0, const char *msg1);
void drawSprite( uint64 color, int x1, int y1, int x2, int y2 );
void drawPopSprite( uint64 color, int x1, int y1, int x2, int y2 );
void drawMsg(const char *msg);
void setupito(int ito_vmode);
void updateScreenMode(void);
void clrScr(uint64 color);
int log(int Value);
void setBrightness(int Brightness);
void loadSkin(int Picture);
void drawScr(void);
void drawFrame(int x1, int y1, int x2, int y2, uint64 color);
void drawChar(unsigned char c, int x, int y, uint64 colour);
int printXY(const unsigned char *s, int x, int y, uint64 colour, int);

/* pad.c */
extern u32 new_pad;
int setupPad(void);
int readpad(void);
int readpad_norepeat(void);
void waitPadReady(int port, int slot);

/* config.c */
#define TV_mode_AUTO 0
#define TV_mode_NTSC 1
#define TV_mode_PAL  2

extern SETTING *setting;
void loadConfig(char *, char *);
void config(char *, char *);

/* filer.c */
unsigned char *elisaFnt;
void getFilePath(char *out, const int cnfmode);
void	initHOST(void);
char *makeHostPath(char *dp, char*sp);
int keyboard(char *out, int max);

/* main.c */
int swapKeys;

#endif
