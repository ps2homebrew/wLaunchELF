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
#include <libkbd.h>
#include <floatlib.h>

#define SCANRATE (ITO_VMODE_AUTO==ITO_VMODE_NTSC ? 60:50)

enum {  // cnfmode values for getFilePath in browsing for configurable file paths
	NON_CNF = 0,     // Normal browser mode, not configuration mode
	LK_ELF_CNF,      // Normal ELF choice for launch keys
	USBD_IRX_CNF,    // USBD.IRX choice for startup
	SKIN_CNF,        // Skin JPG choice
	USBKBD_IRX_CNF,  // USB keyboard IRX choice (only PS2SDK)
  KBDMAP_FILE_CNF, // USB keyboard mapping table choice
  CNF_PATH_CNF,    // CNF Path override choice
  CNFMODE_CNT      // Total number of cnfmode values defined
};

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
	MAX_PARTITIONS=500,
	MAX_MENU_TITLE = 40,
	MAX_ELF_TITLE = 72,
	MAX_TEXT_LINE = 80
};

typedef struct
{
	char CNF_Path[MAX_PATH];
	char LK_Path[15][MAX_PATH];
	char LK_Title[15][MAX_ELF_TITLE];
	char usbd_file[MAX_PATH];
	char usbkbd_file[MAX_PATH];
	char kbdmap_file[MAX_PATH];
	char skin[MAX_PATH];
	char Menu_Title[MAX_MENU_TITLE+1];
	int  Menu_Frame;
	int timeout;
	int Hide_Paths;
	uint64 color[8];
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
	int Init_Delay;
	int usbkbd_used;
	int Show_Titles;
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
void drawLastMsg(void);
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
int printXY_sjis(const unsigned char *s, int x, int y, uint64 colour, int);

/* pad.c */
extern u32 new_pad;
int setupPad(void);
int readpad(void);
int readpad_no_KB(void);
int readpad_noRepeat(void);
void waitPadReady(int port, int slot);
void waitAnyPadReady(void);

/* config.c */
#define TV_mode_AUTO 0
#define TV_mode_NTSC 1
#define TV_mode_PAL  2

extern char PathPad[30][MAX_PATH];
extern SETTING *setting;
void loadConfig(char *, char *);
void config(char *, char *);

/* filer.c */
int nparties; //Clearing this causes FileBrowser to refresh party list
unsigned char *elisaFnt;
void getFilePath(char *out, const int cnfmode);
void	initHOST(void);
char *makeHostPath(char *dp, char*sp);
int ynDialog(const char *message);
void nonDialog(const char *message);
int keyboard(char *out, int max);
void genInit(void);
int genFixPath(char *uLE_path, char *gen_path);
int genOpen(char *path, int mode);
int genLseek(int fd, int where, int how);
int genRead(int fd, void *buf, int size);
int genWrite(int fd, void *buf, int size);
int genClose(int fd);
int genDopen(char *path);
int genDclose(int fd);
int mountParty(const char *party);

/* main.c */
int swapKeys;

/* hdd.c */
void hddManager(void);

#endif
