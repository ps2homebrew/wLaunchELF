//--------------------------------------------------------------
//File name:   main.c
//--------------------------------------------------------------
#include "launchelf.h"

//dlanor: I'm correcting all these erroneous 'u8 *name' declarations
//dlanor: They are not pointers at all, but pure block addresses
//dlanor: Thus they should be declared as 'void name'
extern void iomanx_irx;
extern int  size_iomanx_irx;
extern void filexio_irx;
extern int  size_filexio_irx;
extern void ps2dev9_irx;
extern int  size_ps2dev9_irx;
extern void ps2ip_irx;
extern int  size_ps2ip_irx;
extern void ps2smap_irx;
extern int  size_ps2smap_irx;
extern void smsutils_irx;
extern int  size_smsutils_irx;
extern void ps2host_irx;
extern int  size_ps2host_irx;
extern void vmcfs_irx;
extern int  size_vmcfs_irx;
extern void ps2ftpd_irx;
extern int  size_ps2ftpd_irx;
extern void ps2atad_irx;
extern int  size_ps2atad_irx;
extern void ps2hdd_irx;
extern int  size_ps2hdd_irx;
extern void ps2fs_irx;
extern int  size_ps2fs_irx;
extern void poweroff_irx;
extern int  size_poweroff_irx;
extern void loader_elf;
extern int  size_loader_elf;
extern void ps2netfs_irx;
extern int  size_ps2netfs_irx;
extern void iopmod_irx;
extern int  size_iopmod_irx;
extern void usbd_irx;
extern int  size_usbd_irx;
extern void usb_mass_irx;
extern int  size_usb_mass_irx;
extern void cdvd_irx;
extern int  size_cdvd_irx;
extern void ps2kbd_irx;
extern int  size_ps2kbd_irx;
extern void hdl_info_irx;
extern int  size_hdl_info_irx;

//#define DEBUG
#ifdef DEBUG
#define dbgprintf(args...) scr_printf(args)
#define dbginit_scr() init_scr()
#else
#define dbgprintf(args...) do { } while(0)
#define dbginit_scr() do { } while(0)
#endif

enum
{
	BUTTON,
	DPAD
};

void Reset();

int TV_mode;
int trayopen=FALSE;
int selected=0;
int timeout=0;
int init_delay=0;
int poweroff_delay=0; //Set only when calling hddPowerOff
int mode=BUTTON;
int user_acted = 0;  /* Set when commands given, to break timeout */
char LaunchElfDir[MAX_PATH], mainMsg[MAX_PATH];
char CNF[MAX_NAME];
int numCNF=0;
int maxCNF;
int swapKeys;
int GUI_active;

u64 WaitTime;

#define IPCONF_MAX_LEN  (3*16)
char if_conf[IPCONF_MAX_LEN];
int if_conf_len;

char ip[16]      = "192.168.0.10";
char netmask[16] = "255.255.255.0";
char gw[16]      = "192.168.0.1";

char netConfig[IPCONF_MAX_LEN+64];	//Adjust size as needed

//State of module collections
int have_NetModules = 0;
int have_HDD_modules = 0;
//State of sbv_patches
int have_sbv_patches = 0;
//Old State of Checkable Modules (valid header)
int	old_sio2man  = 0;
int	old_mcman    = 0;
int	old_mcserv   = 0;
int	old_padman   = 0;
int old_fakehost = 0;
int old_poweroff = 0;
int	old_iomanx   = 0;
int	old_filexio  = 0;
int	old_ps2dev9  = 0;
int	old_ps2ip    = 0;
int	old_ps2atad  = 0;
int old_ps2hdd   = 0;
int old_ps2fs    = 0;
int old_ps2netfs = 0;
//State of Uncheckable Modules (invalid header)
int	have_cdvd     = 0;
int	have_usbd     = 0;
int	have_usb_mass = 0;
int	have_ps2smap  = 0;
int	have_ps2host  = 0;
int have_vmcfs    = 0; //vmcfs may be checkable. (must ask Polo)
int	have_ps2ftpd  = 0;
int	have_ps2kbd   = 0;
int	have_hdl_info = 0;
//State of Checkable Modules (valid header)
int have_urgent   = 0;	//flags presence of urgently needed modules
int	have_sio2man  = 0;
int	have_mcman    = 0;
int	have_mcserv   = 0;
int	have_padman   = 0;
int have_fakehost = 0;
int have_poweroff = 0;
int	have_iomanx   = 0;
int	have_filexio  = 0;
int	have_ps2dev9  = 0;
int	have_ps2ip    = 0;
int	have_ps2atad  = 0;
int have_ps2hdd   = 0;
int have_ps2fs    = 0;
int have_ps2netfs = 0;

int force_IOP = 0; //flags presence of incompatible drivers, so we must reset IOP

int menu_LK[15];  //holds RunElf index for each valid main menu entry

int done_setupPowerOff = 0;
int ps2kbd_opened = 0;

int boot_argc;
char *boot_argv[8];
char boot_path[MAX_PATH];

//--------------------------------------------------------------
//executable code
//--------------------------------------------------------------
//Function to print a text row to the 'gs' screen
//------------------------------
int	PrintRow(int row_f, char *text_p)
{	static int row;
	int x = (Menu_start_x + 4);
	int y;

	if(row_f >= 0) row = row_f;
	y = (Menu_start_y + FONT_HEIGHT*row++);
	printXY(text_p, x, y, setting->color[3], TRUE, 0);
	return row;
}  
//------------------------------
//endfunc PrintRow
//--------------------------------------------------------------
//Function to show a screen with debugging info
//------------------------------
void ShowDebugInfo(void)
{	char uLE_rel_path[MAX_PATH], uLE_gen_path[MAX_PATH], TextRow[256];
	int	i, event, post_event=0;

	i = uLE_related(uLE_rel_path, "uLE:/LAUNCHELF.CNF");
	genFixPath("uLE:/LAUNCHELF.CNF", uLE_gen_path);

	event = 1;   //event = initial entry
	//----- Start of event loop -----
	while(1) {
		//Pad response section
		waitAnyPadReady();
		if(readpad() && new_pad){
			event |= 2;
			if (setting->GUI_skin[0]) {
				GUI_active = 1;
				loadSkin(BACKGROUND_PIC, 0, 0);
			}
			break;
		}

		//Display section
		if(event||post_event) { //NB: We need to update two frame buffers per event
			clrScr(setting->color[0]);
			PrintRow(0,"Debug Info Screen:");
			sprintf(TextRow, "argc == %d", boot_argc);
			PrintRow(1,TextRow);
			for(i=0; (i<boot_argc)&&(i<8); i++){
				sprintf(TextRow, "argv[%d] == \"%s\"", i, boot_argv[i]);
				PrintRow(-1, TextRow);
			}
			sprintf(TextRow, "boot_path == \"%s\"", boot_path);
			PrintRow(-1, TextRow);
			sprintf(TextRow, "LaunchElfDir == \"%s\"", LaunchElfDir);
			PrintRow(-1, TextRow);
		}//ends if(event||post_event)
		drawScr();
		post_event = event;
		event = 0;
	} //ends while
	//----- End of event loop -----
}
//------------------------------
//endfunc ShowDebugInfo
//--------------------------------------------------------------
//Function to check for presence of key modules
//------------------------------
void	CheckModules(void)
{	smod_mod_info_t	mod_t;

	old_sio2man  = (have_sio2man = smod_get_mod_by_name(IOPMOD_NAME_SIO2MAN, &mod_t));
	old_mcman    = (have_mcman = smod_get_mod_by_name(IOPMOD_NAME_MCMAN, &mod_t));
	old_mcserv   = (have_mcserv = smod_get_mod_by_name(IOPMOD_NAME_MCSERV, &mod_t));
	old_padman   = (have_padman = smod_get_mod_by_name(IOPMOD_NAME_PADMAN, &mod_t));
	old_fakehost = (have_fakehost = smod_get_mod_by_name(IOPMOD_NAME_FAKEHOST, &mod_t));
	old_poweroff = (have_poweroff = smod_get_mod_by_name(IOPMOD_NAME_POWEROFF, &mod_t));
	old_iomanx   = (have_iomanx = smod_get_mod_by_name(IOPMOD_NAME_IOMANX, &mod_t));
	old_filexio  = (have_filexio = smod_get_mod_by_name(IOPMOD_NAME_FILEXIO, &mod_t));
	old_ps2dev9  = (have_ps2dev9 = smod_get_mod_by_name(IOPMOD_NAME_PS2DEV9, &mod_t));
	old_ps2ip    = (have_ps2ip = smod_get_mod_by_name(IOPMOD_NAME_PS2IP, &mod_t));
	old_ps2atad  = (have_ps2atad = smod_get_mod_by_name(IOPMOD_NAME_PS2ATAD, &mod_t));
	old_ps2hdd   = (have_ps2hdd = smod_get_mod_by_name(IOPMOD_NAME_PS2HDD, &mod_t));
	old_ps2fs    = (have_ps2fs = smod_get_mod_by_name(IOPMOD_NAME_PS2FS, &mod_t));
	old_ps2netfs = (have_ps2netfs= smod_get_mod_by_name(IOPMOD_NAME_PS2NETFS, &mod_t));
}
//------------------------------
//endfunc CheckModules
//--------------------------------------------------------------
// Parse network configuration from IPCONFIG.DAT
// Now completely rewritten to fix some problems
//------------------------------
static void getIpConfig(void)
{
	int fd;
	int i;
	int len;
	char c;
	char buf[IPCONF_MAX_LEN];
	char path[MAX_PATH];

	if(uLE_related(path, "uLE:/IPCONFIG.DAT")==1)
		fd = genOpen(path, O_RDONLY);
	else
		fd=-1;
	fd = genOpen(path, O_RDONLY);
	if (fd >= 0) 
	{	bzero(buf, IPCONF_MAX_LEN);
		len = genRead(fd, buf, IPCONF_MAX_LEN - 1); //Save a byte for termination
		genClose(fd);
	}

	if	((fd >= 0) && (len > 0))
	{	buf[len] = '\0'; //Ensure string termination, regardless of file content
		for	(i=0; ((c = buf[i]) != '\0'); i++) //Clear out spaces and any CR/LF
			if	((c == ' ') || (c == '\r') || (c == '\n'))
				buf[i] = '\0';
		strncpy(ip, buf, 15);
		i = strlen(ip)+1;
		strncpy(netmask, buf+i, 15);
		i += strlen(netmask)+1;
		strncpy(gw, buf+i, 15);
	}

	bzero(if_conf, IPCONF_MAX_LEN);
	strncpy(if_conf, ip, 15);
	i = strlen(ip) + 1;
	strncpy(if_conf+i, netmask, 15);
	i += strlen(netmask) + 1;
	strncpy(if_conf+i, gw, 15);
	i += strlen(gw) + 1;
	if_conf_len = i;
	sprintf(netConfig, "%s:  %-15s %-15s %-15s", LNG(Net_Config), ip, netmask, gw);

}
//------------------------------
//endfunc getIpConfig
//--------------------------------------------------------------
void setLaunchKeys(void)
{
	if(!setting->LK_Flag[12])
		strcpy(setting->LK_Path[12], setting->Misc_Configure);
	if((maxCNF>1) && !setting->LK_Flag[13])
		strcpy(setting->LK_Path[13], setting->Misc_Load_CNFprev);
	if((maxCNF>1) && !setting->LK_Flag[14])
		strcpy(setting->LK_Path[14], setting->Misc_Load_CNFnext);
}
//------------------------------
//endfunc setLaunchKeys()
//--------------------------------------------------------------
int drawMainScreen(void)
{
	int nElfs=0;
	int i;
	int x, y;
	u64 color;
	char c[MAX_PATH+8], f[MAX_PATH];
	char *p;

	setLaunchKeys();

	clrScr(setting->color[0]);

	x = Menu_start_x;
	y = Menu_start_y;
	c[0] = 0;
	if(init_delay)    sprintf(c, "%s: %d", LNG(Init_Delay), init_delay/SCANRATE);
	else if(setting->LK_Path[0][0]){
		if(!user_acted) sprintf(c, "%s: %d", LNG(TIMEOUT), timeout/SCANRATE);
		else            sprintf(c, "%s: %s", LNG(TIMEOUT), LNG(Halted));
	}
	if(c[0]){
		printXY(c, x, y, setting->color[3], TRUE, 0);
		y += FONT_HEIGHT*2;
	}
	for(i=0; i<15; i++){
		if((setting->LK_Path[i][0]) && ((i<13) || (maxCNF>1) || setting->LK_Flag[i]))
		{
			menu_LK[nElfs] = i; //memorize RunElf index for this menu entry
			switch(i){
			case 0:
				strcpy(c,"Default: ");
				break;
			case 1:
				strcpy(c,"     ÿ0: ");
				break;
			case 2:
				strcpy(c,"     ÿ1: ");
				break;
			case 3:
				strcpy(c,"     ÿ2: ");
				break;
			case 4:
				strcpy(c,"     ÿ3: ");
				break;
			case 5:
				strcpy(c,"     L1: ");
				break;
			case 6:
				strcpy(c,"     R1: ");
				break;
			case 7:
				strcpy(c,"     L2: ");
				break;
			case 8:
				strcpy(c,"     R2: ");
				break;
			case 9:
				strcpy(c,"     L3: ");
				break;
			case 10:
				strcpy(c,"     R3: ");
				break;
			case 11:
				strcpy(c,"  START: ");
				break;
			case 12:
				strcpy(c," SELECT: ");
				break;
			case 13:
				sprintf(c,"%s: ", LNG(LEFT));
				break;
			case 14:
				sprintf(c,"%s: ", LNG(RIGHT));
				break;
			} //ends switch
			if(setting->Show_Titles) //Show Launch Titles ?
				strcpy(f, setting->LK_Title[i]);
			else
				f[0] = '\0';
			if(!f[0]) {  //No title present, or allowed ?
				if(setting->Hide_Paths) {  //Hide full path ?
					if((p=strrchr(setting->LK_Path[i], '/'))) // found delimiter ?
						strcpy(f, p+1);
					else // No delimiter !
						strcpy(f, setting->LK_Path[i]);
					if((p=strrchr(f, '.')))
						*p = 0;
				} else {                  //Show full path !
					strcpy(f, setting->LK_Path[i]);
				}
			} //ends clause for No title
			if(nElfs++==selected && mode==DPAD)
				color = setting->color[2];
			else
				color = setting->color[3];
			int len = (strlen(LNG(LEFT))+2>strlen(LNG(RIGHT))+2)?
				strlen(LNG(LEFT))+2:strlen(LNG(RIGHT))+2;
			if(i==13){ // LEFT
				if(strlen(LNG(RIGHT))+2>strlen(LNG(LEFT))+2)
					printXY(c, x+(strlen(LNG(RIGHT))+2>9?
						((strlen(LNG(RIGHT))+2)-(strlen(LNG(LEFT))+2))*FONT_WIDTH:
						(9-(strlen(LNG(LEFT))+2))*FONT_WIDTH), y, color, TRUE, 0);
				else
					printXY(c, x+(strlen(LNG(LEFT))+2>9?
						0:(9-(strlen(LNG(LEFT))+2))*FONT_WIDTH), y, color, TRUE, 0);
			}else if (i==14){ // RIGHT
				if(strlen(LNG(LEFT))+2>strlen(LNG(RIGHT))+2)
					printXY(c, x+(strlen(LNG(LEFT))+2>9?
						((strlen(LNG(LEFT))+2)-(strlen(LNG(RIGHT))+2))*FONT_WIDTH:
						(9-(strlen(LNG(RIGHT))+2))*FONT_WIDTH), y, color, TRUE, 0);
				else
					printXY(c, x+(strlen(LNG(RIGHT))+2>9?
						0:(9-(strlen(LNG(RIGHT))+2))*FONT_WIDTH), y, color, TRUE, 0);
			}else
				printXY(c, x+(len>9? (len-9)*FONT_WIDTH:0), y, color, TRUE, 0);
			printXY(f, x+(len>9? len*FONT_WIDTH:9*FONT_WIDTH), y, color, TRUE, 0);
			y += FONT_HEIGHT;
		} //ends clause for defined LK_Path[i] valid for menu
	} //ends for

	if(mode==BUTTON)	sprintf(c, "%s!", LNG(PUSH_ANY_BUTTON_or_DPAD));
	else{
		if(swapKeys) 
			sprintf(c, "ÿ1:%s ÿ0:%s", LNG(OK), LNG(Cancel));
		else
			sprintf(c, "ÿ0:%s ÿ1:%s", LNG(OK), LNG(Cancel));
	}
	
	setScrTmp(mainMsg, c);
	
	return nElfs;
}
//------------------------------
//endfunc drawMainScreen
//--------------------------------------------------------------
int drawMainScreen2(int TV_mode)
{
	int nElfs=0;
	int i;
	int x, y, xo_config, yo_config, yo_first, yo_step;
	u64 color;
	char c[MAX_PATH+8], f[MAX_PATH];
	char *p;

	setLaunchKeys();

	clrScr(setting->color[0]);

	x = Menu_start_x;
	y = Menu_start_y;

	if(init_delay)    sprintf(c, "%s:       %d", LNG(Delay), init_delay/SCANRATE);
	else if(setting->LK_Path[0][0]){
		if(!user_acted) sprintf(c, "%s:     %d", LNG(TIMEOUT), timeout/SCANRATE);
		else            sprintf(c, "%s:    %s", LNG(TIMEOUT), LNG(Halt));
	}

	if(TV_mode == TV_mode_PAL){
		printXY(c, x+448, y+FONT_HEIGHT+6, setting->color[3], TRUE, 0);
		y += FONT_HEIGHT+5;
		yo_first = 5;
		yo_step = FONT_HEIGHT*2;
		yo_config = -92;
		xo_config = 370;
	}else if(TV_mode == TV_mode_NTSC){
		printXY(c, x+448, y+FONT_HEIGHT-5, setting->color[3], TRUE, 0);
		y += FONT_HEIGHT-3;
		yo_first = 3;
		yo_step = FONT_HEIGHT*2-4;
		yo_config = -80;
		xo_config = 360;
	}

	for(i=0; i<15; i++){
		if((setting->LK_Path[i][0]) && ((i<13)||(maxCNF>1)||setting->LK_Flag[i]))
		{
			menu_LK[nElfs] = i; //memorize RunElf index for this menu entry
			if(setting->Show_Titles) //Show Launch Titles ?
				strcpy(f, setting->LK_Title[i]);
			else
				f[0] = '\0';
			if(!f[0]) {  //No title present, or allowed ?
				if(setting->Hide_Paths) {  //Hide full path ?
					if((p=strrchr(setting->LK_Path[i], '/'))) // found delimiter ?
						strcpy(f, p+1);
					else // No delimiter !
						strcpy(f, setting->LK_Path[i]);
					if((p=strrchr(f, '.')))
						*p = 0;
				} else {                  //Show full path !
					strcpy(f, setting->LK_Path[i]);
				}
			} //ends clause for No title
			if(setting->LK_Path[i][0] && nElfs++==selected && mode==DPAD)
				color = setting->color[2];
			else
				color = setting->color[3];
			int len = (strlen(LNG(LEFT))+2>strlen(LNG(RIGHT))+2)?
				strlen(LNG(LEFT))+2:strlen(LNG(RIGHT))+2;
			if (i==0)
				printXY(f, x+(len>9? len*FONT_WIDTH:9*FONT_WIDTH)+20, y, color, TRUE, 0);
			else if (i==12)
				printXY(f, x+(len>9? len*FONT_WIDTH:9*FONT_WIDTH)+xo_config, y, color, TRUE, 0);
			else if (i==13)
				printXY(f, x+(len>9? len*FONT_WIDTH:9*FONT_WIDTH)+xo_config, y, color, TRUE, 0);
			else if (i==14)
				printXY(f, x+(len>9? len*FONT_WIDTH:9*FONT_WIDTH)+xo_config, y, color, TRUE, 0);
			else
				printXY(f, x+(len>9? len*FONT_WIDTH:9*FONT_WIDTH)+10, y, color, TRUE, 0);
		} //ends clause for defined LK_Path[i] valid for menu
		y += yo_step;
		if (i==0)
			y+=yo_first;
		else if (i==11)
			y+=yo_config;
	} //ends for

	c[0] = '\0';           //dummy tooltip string (Tooltip unused for GUI menu)
	setScrTmp(mainMsg, c);
	
	return nElfs;
}
//------------------------------
//endfunc drawMainScreen2
//--------------------------------------------------------------
void delay(int count)
{
	int i;
	int ret;
	for (i  = 0; i < count; i++) {
	        ret = 0x01000000;
		while(ret--) asm("nop\nnop\nnop\nnop");
	}
}
//------------------------------
//endfunc delay
//--------------------------------------------------------------
void initsbv_patches(void)
{
	if(!have_sbv_patches)
	{	dbgprintf("Init MrBrown sbv_patches\n");
		sbv_patch_enable_lmb();
		sbv_patch_disable_prefix_check();
		have_sbv_patches = 1;
	}
}
//------------------------------
//endfunc initsbv_patches
//--------------------------------------------------------------
void	load_iomanx(void)
{
	int ret;

	if	(!have_iomanx)
	{	SifExecModuleBuffer(&iomanx_irx, size_iomanx_irx, 0, NULL, &ret);
		have_iomanx = 1;
	}
}
//------------------------------
//endfunc load_iomanx
//--------------------------------------------------------------
void	load_filexio(void)
{
	int ret;

	if	(!have_filexio)
	{	SifExecModuleBuffer(&filexio_irx, size_filexio_irx, 0, NULL, &ret);
		have_filexio = 1;
	}
}
//------------------------------
//endfunc load_filexio
//--------------------------------------------------------------
void	load_ps2dev9(void)
{
	int ret;

	load_iomanx();
	if	(!have_ps2dev9)
	{	SifExecModuleBuffer(&ps2dev9_irx, size_ps2dev9_irx, 0, NULL, &ret);
		have_ps2dev9 = 1;
	}
}
//------------------------------
//endfunc load_ps2dev9
//--------------------------------------------------------------
void	load_ps2ip(void)
{
	int ret;

	load_ps2dev9();
	if	(!have_ps2ip){
		SifExecModuleBuffer(&smsutils_irx, size_smsutils_irx, 0, NULL, &ret);
		SifExecModuleBuffer(&ps2ip_irx, size_ps2ip_irx, 0, NULL, &ret);
		have_ps2ip = 1;
	}
	if	(!have_ps2smap){
		SifExecModuleBuffer(&ps2smap_irx, size_ps2smap_irx,
		                    if_conf_len, &if_conf[0], &ret);
		have_ps2smap = 1;
	}
}
//------------------------------
//endfunc load_ps2ip
//--------------------------------------------------------------
void	load_ps2atad(void)
{
	int ret;
	static char hddarg[] = "-o" "\0" "4" "\0" "-n" "\0" "20";
	static char pfsarg[] = "-m" "\0" "4" "\0" "-o" "\0" "10" "\0" "-n" "\0" "40";

	load_ps2dev9();
	if	(!have_ps2atad)
	{	SifExecModuleBuffer(&ps2atad_irx, size_ps2atad_irx, 0, NULL, &ret);
		have_ps2atad = 1;
	}
	if	(!have_ps2hdd)
	{	SifExecModuleBuffer(&ps2hdd_irx, size_ps2hdd_irx, sizeof(hddarg), hddarg, &ret);
		have_ps2hdd = 1;
	}
	if	(!have_ps2fs)
	{	SifExecModuleBuffer(&ps2fs_irx, size_ps2fs_irx, sizeof(pfsarg), pfsarg, &ret);
		have_ps2fs = 1;
	}
}
//------------------------------
//endfunc load_ps2atad
//--------------------------------------------------------------
void	load_ps2host(void)
{
	int ret;

	load_ps2ip();
	if	(!have_ps2host)
	{	SifExecModuleBuffer(&ps2host_irx, size_ps2host_irx, 0, NULL, &ret);
		have_ps2host = 1;
	}
}
//------------------------------
//endfunc load_ps2host
//--------------------------------------------------------------
void	load_vmcfs(void)
{
	int ret;

	load_iomanx();
	load_filexio();
	if	(!have_vmcfs)
	{	SifExecModuleBuffer(&vmcfs_irx, size_vmcfs_irx, 0, NULL, &ret);
		have_vmcfs = 1;
	}
}
//------------------------------
//endfunc load_vmcfs
//--------------------------------------------------------------
void	load_ps2ftpd(void)
{
	int 	ret;
	int		arglen;
	char* arg_p;

	arg_p = "-anonymous";
	arglen = strlen(arg_p);

	load_ps2ip();
	if	(!have_ps2ftpd)
	{	SifExecModuleBuffer(&ps2ftpd_irx, size_ps2ftpd_irx, arglen, arg_p, &ret);
		have_ps2ftpd = 1;
	}
}
//------------------------------
//endfunc load_ps2ftpd
//--------------------------------------------------------------
void	load_ps2netfs(void)
{
	int ret;

	load_ps2ip();
	if	(!have_ps2netfs)
	{	SifExecModuleBuffer(&ps2netfs_irx, size_ps2netfs_irx, 0, NULL, &ret);
		have_ps2netfs = 1;
	}
}
//------------------------------
//endfunc load_ps2netfs
//--------------------------------------------------------------
void loadBasicModules(void)
{
	if	(!have_sio2man) {
		SifLoadModule("rom0:SIO2MAN", 0, NULL);
		have_sio2man = 1;
	}
	if	(!have_mcman) {
		SifLoadModule("rom0:MCMAN", 0, NULL);
		have_mcman = 1;
	}
	if	(!have_mcserv) {
		SifLoadModule("rom0:MCSERV", 0, NULL);
		have_mcserv = 1;
	}
	if	(!have_padman) {
		SifLoadModule("rom0:PADMAN", 0, NULL);
		have_padman = 1;
	}
}
//------------------------------
//endfunc loadBasicModules
//--------------------------------------------------------------
void loadCdModules(void)
{
	int ret;
	
	if	(!have_cdvd) {
		SifExecModuleBuffer(&cdvd_irx, size_cdvd_irx, 0, NULL, &ret);
		cdInit(CDVD_INIT_INIT);
		CDVD_Init();
		have_cdvd = 1;
	}
}
//------------------------------
//endfunc loadCdModules
//--------------------------------------------------------------
// loadExternalFile below will use the given path, and read the
// indicated file into a buffer it allocates for that purpose.
// The buffer address and size are returned via pointer variables,
// and the size is also returned as function value. Both those
// instances of size will be forced to Zero if any error occurs,
// and in such cases the buffer pointer returned will be NULL.
// NB: Release of the allocated memory block, if/when it is not
// needed anymore, is entirely the responsibility of the caller,
// though, of course, none is allocated if the file is not found.
//--------------------------------------------------------------
int	loadExternalFile(char *argPath, void **fileBaseP, int *fileSizeP)
{ //The first three variables are local variants similar to the arguments
	char filePath[MAX_PATH];
	void *fileBase;
	int fileSize;
	FILE*	File;

	fileBase = NULL;
	fileSize = 0;

	if(!strncmp(argPath, "mass:/", 6)){
		//Loading some module from USB mass:
		//NB: This won't be for USB drivers, due to protection elsewhere
		loadUsbModules();
		strcpy(filePath, "mass:");
		strcat(filePath, argPath+6);
	}else if(!strncmp(argPath, "hdd0:/", 6)){
		//Loading some module from HDD
		char party[MAX_PATH];
		char *p;

		loadHddModules();
		sprintf(party, "hdd0:%s", argPath+6);
		p = strchr(party, '/');
		sprintf(filePath, "pfs0:%s", p);
		*p = 0;
		fileXioMount("pfs0:", party, FIO_MT_RDONLY);
	}else if(!strncmp(argPath, "cdfs", 4)){
		loadCdModules();
		strcpy(filePath, argPath);
		CDVD_FlushCache();
		CDVD_DiskReady(0);
	}else{
		(void) uLE_related(filePath, argPath);
	}
	//Here 'filePath' is a valid path for fio or fileXio operations
	//Which means we can now use generic file I/O
	File = fopen( filePath, "r" );
 	if( File != NULL ) {
		fseek(File, 0, SEEK_END);
		fileSize = ftell(File);
		fseek(File, 0, SEEK_SET);
		if(fileSize) {
			if((fileBase = malloc(fileSize)) > 0 ) {
				fread(fileBase, 1, fileSize, File );
			} else
				fileSize =0;
		}
		fclose(File);
	}
	*fileBaseP = fileBase;
	*fileSizeP = fileSize;
	return fileSize;
}
//------------------------------
//endfunc loadExternalFile
//--------------------------------------------------------------
// loadExternalModule below will use the given path and attempt
// to load the indicated file into a temporary buffer, and from
// that buffer send it on to the IOP as a driver module, after
// which the temporary buffer will be freed. If the file is not
// found, or some error occurs in its reading or buffer allocation
// then the default internal module specified by the 2nd and 3rd
// arguments will be used, except if the base is NULL or the size
// is zero, in which case a value of 0 is returned. A value of
// 0 is also returned if loading of default module fails. But
// normally the value returned will be the size of the module.
//--------------------------------------------------------------
int loadExternalModule(char *modPath, void *defBase, int defSize)
{
	void *extBase;
	int extSize;
	int external;       //Flags loading of external file into buffer
	int ext_OK, def_OK; //Flags success for external and default module
	int	dummy;

	ext_OK = (def_OK = 0);
	if( (!(external = loadExternalFile(modPath, &extBase, &extSize)))
		||((ext_OK = SifExecModuleBuffer(extBase, extSize, 0, NULL, &dummy)) < 0) ) {
		if(defBase && defSize) {
			def_OK = SifExecModuleBuffer(defBase, defSize, 0, NULL, &dummy);
		}
	}
	if(external) free(extBase);
	if(ext_OK) return extSize;
	if(def_OK) return defSize;
	return 0;
}
//------------------------------
//endfunc loadExternalModule
//--------------------------------------------------------------
void loadUsbDModule(void)
{
	if( (!have_usbd)
		&&(loadExternalModule(setting->usbd_file, &usbd_irx, size_usbd_irx))
	) have_usbd = 1;
}
//------------------------------
//endfunc loadUsbDModule
//--------------------------------------------------------------
void loadUsbModules(void)
{
	//int ret;

	loadUsbDModule();
	if(	have_usbd
	&&	!have_usb_mass
	&&	loadExternalModule(setting->usbmass_file, &usb_mass_irx, size_usb_mass_irx)){
		delay(3);
		//ret = usb_mass_bindRpc(); //dlanor: disused in switching to usbhdfsd
		have_usb_mass = 1;
	}
}
//------------------------------
//endfunc loadUsbModules
//--------------------------------------------------------------
void loadKbdModules(void)
{
	loadUsbDModule();
	if( (have_usbd && !have_ps2kbd)
		&&(loadExternalModule(setting->usbkbd_file, &ps2kbd_irx, size_ps2kbd_irx))
	)	have_ps2kbd = 1;
}
//------------------------------
//endfunc loadKbdModules
//--------------------------------------------------------------
void loadHdlInfoModule(void)
{
	int ret;

	if(!have_hdl_info){
		drawMsg(LNG(Loading_HDL_Info_Module));
		SifExecModuleBuffer(&hdl_info_irx, size_hdl_info_irx, 0, NULL, &ret);
		ret = Hdl_Info_BindRpc();
		have_hdl_info = 1;
	}
}
//------------------------------
//endfunc loadHdlInfoModule
//--------------------------------------------------------------
void poweroffHandler(int i)
{
	hddPowerOff();
}
//------------------------------
//endfunc poweroffHandler
//--------------------------------------------------------------
void setupPowerOff(void) {
	int ret;

	if(!done_setupPowerOff) {
		hddPreparePoweroff();
		hddSetUserPoweroffCallback((void *)poweroffHandler, NULL);
		if	(!have_poweroff) {
			SifExecModuleBuffer(&poweroff_irx, size_poweroff_irx, 0, NULL, &ret);
			have_poweroff = 1;
		}
		load_iomanx();
		load_filexio();
		load_ps2dev9();
		done_setupPowerOff = 1;
	}
}
//------------------------------
//endfunc setupPowerOff
//--------------------------------------------------------------
void loadHddModules(void)
{
	if(!have_HDD_modules) {
		drawMsg(LNG(Loading_HDD_Modules));
		setupPowerOff();
		load_ps2atad(); //also loads ps2hdd & ps2fs
		have_HDD_modules = TRUE;
	}
}
//------------------------------
//endfunc loadHddModules
//--------------------------------------------------------------
// Load Network modules by EP (modified by RA)
//------------------------------
void loadNetModules(void)
{
	if(!have_NetModules){
		loadHddModules();
		loadUsbModules();
		drawMsg(LNG(Loading_NetFS_and_FTP_Server_Modules));
		
		getIpConfig(); //RA NB: I always get that info, early in init
		//             //But sometimes it is useful to do it again (HDD)
		// Also, my module checking makes some other tests redundant
		load_ps2netfs(); // loads ps2netfs from internal buffer
		load_ps2ftpd();  // loads ps2dftpd from internal buffer
		have_NetModules = 1;
	}
	strcpy(mainMsg, netConfig);
	if (setting->GUI_skin[0]) {
		GUI_active = 1;
		loadSkin(BACKGROUND_PIC, 0, 0);
	}
}
//------------------------------
//endfunc loadNetModules
//--------------------------------------------------------------
void startKbd(void)
{
	int kbd_fd;
	void *mapBase;
	int   mapSize;

	printf("Entering startKbd()\r\n");
	if(setting->usbkbd_used) {
		loadKbdModules();
		PS2KbdInit();
		ps2kbd_opened = 1;
		if(setting->kbdmap_file[0]) {
			if((kbd_fd = fioOpen(PS2KBD_DEVFILE, O_RDONLY)) >= 0) {
				printf("kbd_fd=%d; Loading Kbd map file \"%s\"\r\n",kbd_fd,setting->kbdmap_file);
				if(loadExternalFile(setting->kbdmap_file, &mapBase, &mapSize)) {
					if(mapSize == 0x600) {
						fioIoctl(kbd_fd, PS2KBD_IOCTL_SETKEYMAP, mapBase);
						fioIoctl(kbd_fd, PS2KBD_IOCTL_SETSPECIALMAP, mapBase+0x300);
						fioIoctl(kbd_fd, PS2KBD_IOCTL_SETCTRLMAP, mapBase+0x400);
						fioIoctl(kbd_fd, PS2KBD_IOCTL_SETALTMAP, mapBase+0x500);
					}
					printf("Freeing buffer after setting Kbd maps\r\n");
					free(mapBase);
				}
				fioClose(kbd_fd);
			}
		}
	}
}
//------------------------------
//endfunc startKbd
//--------------------------------------------------------------
// Read SYSTEM.CNF for MISC/PS2Disc launch command
int ReadCNF(char *LK_Path)
{
	char *systemcnf;
	int fd;
	int size;
	int n;
	int i;
	
	loadCdModules();
	/*
	CDVD_FlushCache();
	CDVD_DiskReady(0);
	*/
	i = 0x10000;
	while(i--) asm("nop\nnop\nnop\nnop");
	fd = fioOpen("cdrom0:\\SYSTEM.CNF;1",1);
	if(fd>=0) {
		size = fioLseek(fd,0,SEEK_END);
		fioLseek(fd,0,SEEK_SET);
		systemcnf = (char*)malloc(size+1);
		fioRead(fd, systemcnf, size);
		systemcnf[size]=0; //RA NB: [size] means the first byte after file bytes
		//RA NB: An old error used systemcnf[size+1] above
		//RA NB: which zeroes one byte beyond the allocated buffer
		//RA NB: leaving an unknown byte to be accepted at buffer end
		//RA NB: That byte may be zeroed by malloc, but we shouldn't rely on it
		for(n=0; systemcnf[n]!=0; n++){
			if(!strncmp(&systemcnf[n], "BOOT2", 5)) {
				n+=5;
				break;
			}
		}
		while(systemcnf[n]!=0 && systemcnf[n]==' ') n++; //skip spaces
		if(systemcnf[n]!=0 ) n++;                        //skip '='
		while(systemcnf[n]!=0 && systemcnf[n]==' ') n++; //skip spaces
		if(systemcnf[n]==0){ //termination without any pathname
			free(systemcnf);
			return 0;
		}
		
		for(i=0; systemcnf[n+i]!=0; i++) {
			LK_Path[i] = systemcnf[n+i];
			if(i>2)
				if(!strncmp(&LK_Path[i-1], ";1", 2)) {
					LK_Path[i+1]=0;
					break;
				}
		}
		fioClose(fd);
		free(systemcnf);
		return 1;
	}
	return 0;
}
//------------------------------
//endfunc ReadCNF
//--------------------------------------------------------------
void	ShowFont(void)
{
	int test_type=0;
	int test_types=2;  //Patch test_types for number of test loops
	int	i, j, event, post_event=0;
	char Hex[18] = "0123456789ABCDEF";
	int ch_x_stp = 1+FONT_WIDTH+1+LINE_THICKNESS;
	int ch_y_stp = 2+FONT_HEIGHT+1+LINE_THICKNESS;
	int	mat_w = LINE_THICKNESS+17*ch_x_stp;
	int mat_h = LINE_THICKNESS+17*ch_y_stp;
	int mat_x = (((SCREEN_WIDTH-mat_w)/2) & -2);
	int mat_y = (((SCREEN_HEIGHT-mat_h)/2) & -2);
	int ch_x  = mat_x+LINE_THICKNESS+1;
//	int	ch_y  = mat_y+LINE_THICKNESS+2;
	int px, ly, cy;
	u64 col_0=setting->color[0], col_1=setting->color[1], col_3=setting->color[3];

//The next line is a patch to save font, if/when needed (needs patch in draw.c too)
//	WriteFont_C("mc0:/SYS-CONF/font_uLE.c");

	event = 1;   //event = initial entry
	//----- Start of event loop -----
	while(1) {
		//Display section
		if(event||post_event) { //NB: We need to update two frame buffers per event
			drawOpSprite(col_0, mat_x, mat_y, mat_x+mat_w-1, mat_y+mat_h-1);
			//Here the background rectangle has been prepared
/* //Start of commented out section //Move this line as needed for tests
			//Start of gsKit test section
			if(test_type > 1) goto done_test;
			gsKit_prim_point(gsGlobal, mat_x+16, mat_y+16, 1, col_3);
			gsKit_prim_point(gsGlobal, mat_x+33, mat_y+16, 1, col_3);
			gsKit_prim_point(gsGlobal, mat_x+33, mat_y+33, 1, col_3);
			gsKit_prim_point(gsGlobal, mat_x+16, mat_y+33, 1, col_3);
			gsKit_prim_line(gsGlobal, mat_x+48, mat_y+48, mat_x+65, mat_y+48, 1, col_3);
			gsKit_prim_line(gsGlobal, mat_x+65, mat_y+48, mat_x+65, mat_y+65, 1, col_3);
			gsKit_prim_line(gsGlobal, mat_x+65, mat_y+65, mat_x+48, mat_y+65, 1, col_3);
			gsKit_prim_line(gsGlobal, mat_x+48, mat_y+65, mat_x+48, mat_y+48, 1, col_3);
			gsKit_prim_sprite(gsGlobal, mat_x+80, mat_y+80, mat_x+97, mat_y+81, 1, col_3);
			gsKit_prim_sprite(gsGlobal, mat_x+97, mat_y+80, mat_x+96, mat_y+97, 1, col_3);
			gsKit_prim_sprite(gsGlobal, mat_x+97, mat_y+97, mat_x+80, mat_y+96, 1, col_3);
			gsKit_prim_sprite(gsGlobal, mat_x+80, mat_y+97, mat_x+81, mat_y+80, 1, col_3);
			gsKit_prim_line(gsGlobal, mat_x+80, mat_y+16, mat_x+81, mat_y+16, 1, col_3);
			gsKit_prim_line(gsGlobal, mat_x+97, mat_y+16, mat_x+97, mat_y+17, 1, col_3);
			gsKit_prim_line(gsGlobal, mat_x+97, mat_y+33, mat_x+96, mat_y+33, 1, col_3);
			gsKit_prim_line(gsGlobal, mat_x+80, mat_y+33, mat_x+80, mat_y+32, 1, col_3);
			gsKit_prim_sprite(gsGlobal, mat_x+16, mat_y+80, mat_x+17, mat_y+81, 1, col_3);
			gsKit_prim_sprite(gsGlobal, mat_x+33, mat_y+80, mat_x+32, mat_y+81, 1, col_3);
			gsKit_prim_sprite(gsGlobal, mat_x+33, mat_y+97, mat_x+32, mat_y+96, 1, col_3);
			gsKit_prim_sprite(gsGlobal, mat_x+16, mat_y+97, mat_x+17, mat_y+96, 1, col_3);
			goto end_display;
done_test:
			//End of gsKit test section
*/ //End of commented out section  //Move this line as needed for tests
			//Start of font display section
			//Now we start to draw all vertical frame lines
			px=mat_x;
			drawOpSprite(col_1, px, mat_y, px+LINE_THICKNESS-1, mat_y+mat_h-1);
			for(j=0; j<17; j++) { //for each font column, plus the row_index column
				px += ch_x_stp;
				drawOpSprite(col_1, px, mat_y, px+LINE_THICKNESS-1, mat_y+mat_h-1);
			} //ends for each font column, plus the row_index column
			//Here all the vertical frame lines have been drawn
			//Next we draw the top horizontal line
			drawOpSprite(col_1, mat_x, mat_y, mat_x+mat_w-1, mat_y+LINE_THICKNESS-1);
			cy = mat_y+LINE_THICKNESS+2;
			ly = mat_y;
			for(i=0; i<17; i++) { //for each font row
				px=ch_x;
				if(!i) { //if top row (which holds the column indexes)
					drawChar('\\', px, cy, col_3);     //Display '\' at index crosspoint
				} else { //else a real font row
					drawChar(Hex[i-1], px, cy, col_3); //Display row index
				}
				for(j=0; j<16; j++) { //for each font column
					px += ch_x_stp;
					if(!i) { //if top row (which holds the column indexes)
						drawChar(Hex[j], px, cy, col_3); //Display Column index
					} else {
						drawChar((i-1)*16+j, px, cy, col_3); //Display font character
					}
				} //ends for each font column
				ly += ch_y_stp;
				drawOpSprite(col_1, mat_x, ly, mat_x+mat_w-1, ly+LINE_THICKNESS-1);
				cy += ch_y_stp;
			} //ends for each font row
			//End of font display section
		} //ends if(event||post_event)
//end_display:
		drawScr();
		post_event = event;
		event = 0;

		//Pad response section
		waitAnyPadReady();
		if(readpad() && new_pad){
			event |= 2;
			if((++test_type) < test_types){
				mat_y++;
				continue;
			}
			if (setting->GUI_skin[0]) {
				GUI_active = 1;
				loadSkin(BACKGROUND_PIC, 0, 0);
			}
			break;
		}
	} //ends while
	//----- End of event loop -----
}
//------------------------------
//endfunc ShowFont
//--------------------------------------------------------------
void triggerPowerOff(void)
{
	char filepath[MAX_PATH] = "xyz:/imaginary/hypothetical/doesn't.exist";
	FILE *File;

	File = fopen( filepath, "r" );
//	sprintf(mainMsg, "%s => %08X.", filepath, File);
//	drawMsg(mainMsg);
	if( File != NULL ) {
		fclose( File );
	} // end if( File != NULL )
}
//------------------------------
//endfunc triggerPowerOff
//--------------------------------------------------------------
void Validate_CNF_Path(void)
{
	char cnf_path[MAX_PATH];

	if(setting->CNF_Path[0] != '\0') {
		if(genFixPath(setting->CNF_Path, cnf_path) >= 0)
			strcpy(LaunchElfDir, setting->CNF_Path);
	}
}
//------------------------------
//endfunc Validate_CNF_Path
//--------------------------------------------------------------
void Set_CNF_Path(void)
{
	char	*tmp;

	getFilePath(setting->CNF_Path, CNF_PATH_CNF);
	if((tmp = strrchr(setting->CNF_Path, '/')))
	tmp[1] = '\0';
	Validate_CNF_Path();

	if(!strcmp(setting->CNF_Path, LaunchElfDir))
		sprintf(mainMsg, "%s ", LNG(Valid));
	else
		sprintf(mainMsg, "%s ", LNG(Bogus));
	sprintf(mainMsg+6, "%s = \"%s\"", LNG(CNF_Path), setting->CNF_Path);

}
//------------------------------
//endfunc Set_CNF_Path
//--------------------------------------------------------------
//Reload CNF, possibly after a path change
int reloadConfig()
{
	char tmp[MAX_PATH];
	int CNF_error = -1;

	if (numCNF == 0)
		strcpy(CNF, "LAUNCHELF.CNF");
	else
		sprintf(CNF, "LAUNCHELF%i.CNF", numCNF);

	CNF_error = loadConfig(mainMsg, CNF);
	Validate_CNF_Path();
	updateScreenMode(0);
	if (setting->GUI_skin[0]) GUI_active = 1;
	else GUI_active= 0;
	loadSkin(BACKGROUND_PIC, 0, 0);

	if(CNF_error<0)
		strcpy(tmp, mainMsg+strlen(LNG(Failed_To_Load)));
	else
		strcpy(tmp, mainMsg+strlen(LNG(Loaded_Config)));

	Load_External_Language();
	loadFont(setting->font_file);

	if(CNF_error<0)
		sprintf(mainMsg, "%s%s", LNG(Failed_To_Load), tmp);
	else
		sprintf(mainMsg, "%s%s", LNG(Loaded_Config), tmp);

	timeout = (setting->timeout+1)*SCANRATE;
	if(setting->discControl)
		loadCdModules();

	return CNF_error;
}
//------------------------------
//endfunc reloadConfig
//--------------------------------------------------------------
// Config Cycle Left  (--) by EP
void decConfig()
{
	if (numCNF > 0)
		numCNF--;
	else
		numCNF = maxCNF-1;
	
	reloadConfig();
}
//------------------------------
//endfunc decConfig
//--------------------------------------------------------------
// Config Cycle Right (++) by EP
void incConfig()
{
	if (numCNF < maxCNF-1)
		numCNF++;
	else
		numCNF = 0;
	
	reloadConfig();
}
//------------------------------
//endfunc incConfig
//--------------------------------------------------------------
//exists.  Tests if a file exists or not
//------------------------------
int exists(char *path)
{
	int fd;

	fd = genOpen(path, O_RDONLY);
	if( fd < 0 )
		return 0;
	genClose(fd);
	return 1;
}
//------------------------------
//endfunc exists
//--------------------------------------------------------------
//uLE_related.  Tests if an uLE_related file exists or not
// Returns:
//  1 == uLE related path with file present
//  0 == uLE related path with file missing
// -1 == Not uLE related path
//------------------------------
int uLE_related(char *pathout, char *pathin)
{
	int ret;

	if(!strncmp(pathin, "uLE:/", 5)){
		sprintf(pathout, "%s%s", LaunchElfDir, pathin+5);
		if( exists(pathout) )
			return 1;
		sprintf(pathout, "%s%s", "mc0:/SYS-CONF/", pathin+5);
		if( !strncmp(LaunchElfDir, "mc1", 3) )
			pathout[2] = '1';
		if( exists(pathout) )
			return 1;
		pathout[2] ^= 1;  //switch between mc0 and mc1
		if( exists(pathout) )
			return 1;
		ret = 0;
	} else
		ret = -1;
	strcpy(pathout, pathin);
	return ret;
}
//------------------------------
//endfunc uLE_related
//--------------------------------------------------------------
// Run ELF. The generic ELF launcher.
//------------------------------
void RunElf(char *pathin)
{
	char tmp[MAX_PATH];
	static char path[MAX_PATH];
	static char fullpath[MAX_PATH];
	static char party[40];
	char *p;
	int x, t=0;

	if(pathin[0]==0) return;
	
	if( !uLE_related(path, pathin) ) //1==uLE_rel 0==missing, -1==other dev
		return;

	if(!strncmp(path, "mc", 2)){
		party[0] = 0;
		if(path[2]!=':')
			goto CheckELF_path;
		strcpy(fullpath, "mc0:");
		strcat(fullpath, path+3);
		if(checkELFheader(fullpath)>0)
			goto ELFchecked;
		fullpath[2]='1';
		goto CheckELF_fullpath;
	}else if(!strncmp(path, "vmc", 3)){
		x = path[3] - '0';
		if((x<0)||(x>1)||!vmcMounted[x])
			goto ELFnotFound;
		goto CheckELF_path;
	}else if(!strncmp(path, "hdd0:/", 6)){
		loadHddModules();
		if((t=checkELFheader(path))<=0)
			goto ELFnotFound;
		//coming here means the ELF is fine
		sprintf(party, "hdd0:%s", path+6);
		p = strchr(party, '/');
		sprintf(fullpath, "pfs0:%s", p);
		*p = 0;
		goto ELFchecked;
	}else if(!strncmp(path, "mass:", 5)){
		loadUsbModules();
		if((t=checkELFheader(path))<=0)
			goto ELFnotFound;
		//coming here means the ELF is fine
		party[0] = 0;
		strcpy(fullpath, "mass:");
		if(path[5] == '/')
			strcat(fullpath, path+6);
		else
			strcat(fullpath, path+5);
		goto ELFchecked;
	}else if(!strncmp(path, "host:", 5)){
		initHOST();
		party[0] = 0;
		strcpy(fullpath, "host:");
		if(path[5] == '/')
			strcat(fullpath, path+6);
		else
			strcat(fullpath, path+5);
		makeHostPath(fullpath, fullpath);
		goto CheckELF_fullpath;
	}else if(!stricmp(path, setting->Misc_PS2Disc)){
		drawMsg(LNG(Reading_SYSTEMCNF));
		strcpy(mainMsg, LNG(Failed));
		party[0]=0;
		trayopen=FALSE;
		if(!ReadCNF(fullpath))
			return;  //This should be extended with a fitting error message
		goto CheckELF_fullpath;
	}else if(!stricmp(path, setting->Misc_FileBrowser)){
		if (setting->GUI_skin[0]) {
			GUI_active = 0;
			loadSkin(BACKGROUND_PIC, 0, 0);
		}
		mainMsg[0] = 0;
		tmp[0] = 0;
		LastDir[0] = 0;
		getFilePath(tmp, FALSE);
		if(tmp[0])
			RunElf(tmp);
		return;
	}else if(!stricmp(path, setting->Misc_PS2Browser)){
		__asm__ __volatile__(
			"	li $3, 0x04;"
			"	syscall;"
			"	nop;"
		);
		//There has been a major change in the code for calling PS2Browser
		//The method above is borrowed from PS2MP3. It's independent of ELF loader
		//The method below was used earlier, but causes reset with new ELF loader
		//party[0]=0;
		//strcpy(fullpath,"rom0:OSDSYS");
	}else if(!stricmp(path, setting->Misc_PS2Net)){
		mainMsg[0] = 0;
		loadNetModules();
		return;
	}else if(!stricmp(path, setting->Misc_PS2PowerOff)){
		mainMsg[0] = 0;
		drawMsg(LNG(Powering_Off_Console));
		setupPowerOff();
		hddPowerOff();
		poweroff_delay = SCANRATE/4; //trigger delay for those without net adapter
		return;
	}else if(!stricmp(path, setting->Misc_HddManager)){
		if (setting->GUI_skin[0]) {
			GUI_active = 0;
			loadSkin(BACKGROUND_PIC, 0, 0);
		}
		hddManager();
		return;
	}else if(!stricmp(path, setting->Misc_TextEditor)){
		if (setting->GUI_skin[0]) {
			GUI_active = 0;
			loadSkin(BACKGROUND_PIC, 0, 0);
		}
		TextEditor();
		return;
	}else if(!stricmp(path, setting->Misc_JpgViewer)){
		if (setting->GUI_skin[0]) {
			GUI_active = 0;
			loadSkin(BACKGROUND_PIC, 0, 0);
		}
		JpgViewer();
		return;
	}else if(!stricmp(path, setting->Misc_Configure)){
		if (setting->GUI_skin[0]) {
			GUI_active = 0;
			loadSkin(BACKGROUND_PIC, 0, 0);
		}
		config(mainMsg, CNF);
		return;
	}else if(!stricmp(path, setting->Misc_Load_CNFprev)){
		decConfig();
		return;
	}else if(!stricmp(path, setting->Misc_Load_CNFnext)){
		incConfig();
		return;
	}else if(!stricmp(path, setting->Misc_Set_CNF_Path)){
		Set_CNF_Path();
		return;
	}else if(!stricmp(path, setting->Misc_Load_CNF)){
		reloadConfig();
		return;
//Next clause is for an optional font test routine
	}else if(!stricmp(path, setting->Misc_ShowFont)){
		ShowFont();
		return;
	}else if(!stricmp(path, setting->Misc_Debug_Info)){
		if (setting->GUI_skin[0]) {
			GUI_active = 0;
			loadSkin(BACKGROUND_PIC, 0, 0);
		}
		ShowDebugInfo();
		return;
	}else if(!strncmp(path, "cdfs", 4)){
		loadCdModules();
		CDVD_FlushCache();
		CDVD_DiskReady(0);
		party[0] = 0;
		goto CheckELF_path;
	}else if(!strncmp(path, "rom", 3)){
		party[0] = 0;
CheckELF_path:
		strcpy(fullpath, path);
CheckELF_fullpath:
		if((t=checkELFheader(fullpath))<=0)
			goto ELFnotFound;
	}else{ //Invalid path
		t = 0;
ELFnotFound:
		if(t==0)
			sprintf(mainMsg, "%s %s.", fullpath, LNG(is_Not_Found));
		else
			sprintf(mainMsg, "%s: %s.", LNG(This_file_isnt_an_ELF), fullpath);
		return;
	}

ELFchecked:
	clrScr(GS_SETREG_RGBA(0x00, 0x00, 0x00, 0));
	drawScr();
	clrScr(GS_SETREG_RGBA(0x00, 0x00, 0x00, 0));
	drawScr();
	free(setting);
	free(elisaFnt);
	free(External_Lang_Buffer);
	free(FontBuffer);
	padPortClose(1,0);
	padPortClose(0,0);
	if(ps2kbd_opened) PS2KbdClose();
	TimerEnd();
	RunLoaderElf(fullpath, party);
}
//------------------------------
//endfunc RunElf
//--------------------------------------------------------------
// reboot IOP (original source by Hermes in BOOT.c - cogswaploader)
// dlanor: but changed now, as the original was badly bugged
void Reset()
{
	SifIopReset("rom0:UDNL rom0:EELOADCNF",0);
	while(!SifIopSync());
	fioExit();
	SifExitIopHeap();
	SifLoadFileExit();
	SifExitRpc();
	SifExitCmd();

	SifInitRpc(0);
	FlushCache(0);
	FlushCache(2);

	have_cdvd     = 0;
	have_usbd     = 0;
	have_usb_mass = 0;
	have_ps2smap  = 0;
	have_ps2host  = 0;
	have_ps2ftpd  = 0;
	have_ps2kbd   = 0;
	have_NetModules = 0;
	have_HDD_modules = 0;
	have_sbv_patches = 0;

	CheckModules();
	loadBasicModules();
	mcReset();
	mcInit(MC_TYPE_MC);
//	padReset();
//	setupPad();
}
//------------------------------
//endfunc Reset
//--------------------------------------------------------------
int main(int argc, char *argv[])
{
	char *p, CNF_pathname[MAX_PATH];
	int event, post_event=0;
	int RunELF_index, nElfs=0;
	CdvdDiscType_t cdmode, old_cdmode;  //used for disc change detection
	int hdd_booted = 0;
	int host_or_hdd_booted = 0;
	int mass_booted = 0; //flags boot made with compatible mass drivers
	int mass_needed = 0; //flags need to load compatible mass drivers
	int mc_booted = 0;
	int cdvd_booted = 0;
	int	host_booted = 0;
	int gs_vmode;
	int CNF_error = -1; //assume error until CNF correctly loaded
	int i;

	boot_argc = argc;
	for(i=0; (i<argc)&&(i<8); i++)
		boot_argv[i] = argv[i];

	SifInitRpc(0);
	CheckModules();
	loadBasicModules();
	mcInit(MC_TYPE_MC);
	genInit();
	Init_Default_Language();

	force_IOP = 0;
	LaunchElfDir[0] = 0;
	if	((argc > 0) && argv[0]){
		strcpy(LaunchElfDir, argv[0]);
		if	(!strncmp(argv[0], "mass", 4)){
			if(!strncmp(argv[0], "mass0:\\", 7)){  //SwapMagic boot path for usb_mass
				//Transform the boot path to homebrew standards
				LaunchElfDir[4]=':';
				strcpy(&LaunchElfDir[5], &LaunchElfDir[7]);
				for(i=0; LaunchElfDir[i]!=0; i++){
					if(LaunchElfDir[i] == '\\')
						LaunchElfDir[i] = '/';
				}
				force_IOP = 1;   //Note incompatible drivers present (as yet ignored)
				mass_needed = 1; //Note need to load compatible mass: drivers
			} else {  //else we booted with normal homebrew mass: drivers
				mass_booted = 1; //Note presence of compatible mass: drivers
			}
		}
		else if	(!strncmp(argv[0], "mc", 2))
			mc_booted = 1;
		else if	(!strncmp(argv[0], "cd", 2))
			cdvd_booted = 1;
		else if	((!strncmp(argv[0], "hdd", 3)) || (!strncmp(argv[0], "pfs", 3)))
			hdd_booted = 1;  //Modify this section later to cover Dev2 needs !!!
	}
	strcpy(boot_path, LaunchElfDir);

	if(!strncmp(LaunchElfDir, "host",4)) {
		host_or_hdd_booted = 1;
		if	(have_fakehost)
			hdd_booted = 1;
		else
			host_booted = 1;
	}

	if	(host_booted)	//Fix untestable modules for host booting
	{	have_ps2smap = 1;
		have_ps2host	= 1;
	}

	if	(mass_booted)	//Fix untestable module for USB_mass booting
	{	have_usbd = 1;
		have_usb_mass = 1;
	}

	if	(	((p=strrchr(LaunchElfDir, '/'))==NULL)
			&&((p=strrchr(LaunchElfDir, '\\'))==NULL)
			)	p=strrchr(LaunchElfDir, ':');
	if	(p!=NULL)
		*(p+1)=0;
	//The above cuts away the ELF filename from LaunchElfDir, leaving a pure path

	if(hdd_booted && !strncmp(LaunchElfDir, "hdd", 3)){;
		//Patch DMS4 Dev2 booting here, when we learn more about how it works
		//Trying to mount that partition for loading CNF simply crashes.
		//We may need a new IOP reset method for this.
	}

	LastDir[0] = 0;

	if(gsKit_detect_signal()==GS_MODE_PAL) {  //Test console TV mode
		SCREEN_X			= 652;
		SCREEN_Y			= 72;
	} else {
		SCREEN_X			= 632;
		SCREEN_Y			= 50;
	}

	//RA NB: loadConfig needs  SCREEN_X and SCREEN_Y to be defaults matching TV mode
	CNF_error = loadConfig(mainMsg, strcpy(CNF, "LAUNCHELF.CNF"));
	if(CNF_error<0)
		strcpy(CNF_pathname, mainMsg+strlen(LNG(Failed_To_Load)));
	else
		strcpy(CNF_pathname, mainMsg+strlen(LNG(Loaded_Config)));

	init_delay = setting->Init_Delay*SCANRATE;

	TV_mode = setting->TV_mode;
	if((TV_mode!=TV_mode_NTSC)&&(TV_mode!=TV_mode_PAL)){ //If no forced request
		if(gsKit_detect_signal()==GS_MODE_PAL)             //Let console decide
			TV_mode = TV_mode_PAL;
		else
			TV_mode = TV_mode_NTSC;
	}

	if(TV_mode == TV_mode_PAL){ //Use PAL mode if chosen (forced or auto)
		gs_vmode = GS_MODE_PAL;
		SCREEN_WIDTH	= 640;
		SCREEN_HEIGHT = 512;
		SCREEN_X			= 652;
		SCREEN_Y			= 72;
		Menu_end_y			= Menu_start_y + 26*FONT_HEIGHT;
	}else{                      //else use NTSC mode (forced or auto)
		gs_vmode = GS_MODE_NTSC;
		SCREEN_WIDTH	= 640;
		SCREEN_HEIGHT = 448;
		SCREEN_X			= 632;
		SCREEN_Y			= 50;
		Menu_end_y		 = Menu_start_y + 22*FONT_HEIGHT;
	} /* end else */
	Frame_end_y			= Menu_end_y + 4;
	Menu_tooltip_y	= Frame_end_y + LINE_THICKNESS + 2;

	maxCNF = setting->numCNF;
	swapKeys = setting->swapKeys;
	if(setting->resetIOP)
	{	Reset();
		if(!strncmp(LaunchElfDir, "mass:", 5))
		{	initsbv_patches();
			loadUsbModules();
		}
		else if(!strncmp(LaunchElfDir, "host:", 5))
		{	getIpConfig();
			initsbv_patches();
			initHOST();
		}
	}
	//Here IOP reset (if done) has been completed, so it's time to load and init drivers
	getIpConfig();
	setupPad(); //Comment out this line when using early setupPad above
	initsbv_patches();

	if(setting->discControl)
		loadCdModules();

//Last chance to look at bootup screen, so allow braking here
/*
	if(readpad() && (new_pad && PAD_UP))
	{ scr_printf("________ Boot paused. Press 'Circle' to continue.\n");
		while(1)
		{	if(new_pad & PAD_CIRCLE)
				break;
			while(!readpad());
		}
	}
*/
	setupGS(gs_vmode);
	gsKit_clear(gsGlobal, GS_SETREG_RGBAQ(0x00,0x00,0x00,0x00,0x00));

	startKbd();
	TimerInit();
	WaitTime=Timer();

	loadFont("");  //Some font must be loaded before loading some device modules
	Load_External_Language();
	loadFont(setting->font_file);
	if (setting->GUI_skin[0]) {GUI_active = 1;}
	loadSkin(BACKGROUND_PIC, 0, 0);

	gsKit_clear(gsGlobal, GS_SETREG_RGBAQ(0x00,0x00,0x00,0x00,0x00));

	if(CNF_error<0)
		sprintf(mainMsg, "%s%s", LNG(Failed_To_Load), CNF_pathname);
	else
		sprintf(mainMsg, "%s%s", LNG(Loaded_Config), CNF_pathname);

	//Here nearly everything is ready for the main menu event loop
	//But before we start that, we need to validate CNF_Path
	Validate_CNF_Path();

	timeout = (setting->timeout+1)*SCANRATE;
	cdmode = -1; //flag unchecked cdmode state
	event = 1;   //event = initial entry
	//----- Start of main menu event loop -----
	while(1){
		//Background event section
		if(setting->discControl){
			CDVD_Stop();
			old_cdmode = cdmode;
			cdmode = cdGetDiscType();
			if(cdmode!=old_cdmode)
				event |= 4;  //event |= disc change detection
			if(cdmode==CDVD_TYPE_NODISK){
				trayopen = TRUE;
				strcpy(mainMsg, LNG(No_Disc));
			}else if(cdmode>=0x01 && cdmode<=0x04){
				strcpy(mainMsg, LNG(Detecting_Disc));
			}else if(trayopen==TRUE){
				trayopen=FALSE;
				strcpy(mainMsg, LNG(Stop_Disc));
			}
		}

		if(poweroff_delay) {
			poweroff_delay--;
			if(!poweroff_delay)
				triggerPowerOff();
		}

		if(init_delay){
			init_delay--;
			if(init_delay%SCANRATE==SCANRATE-1)
				event |= 8;  //event |= visible delay change
		}
		else if(timeout && !user_acted){
			timeout--;
			if(timeout%SCANRATE==SCANRATE-1)
				event |= 8;  //event |= visible timeout change
		}

		//Display section
		if(event||post_event){  //NB: We need to update two frame buffers per event
			if (!(setting->GUI_skin[0]))
				nElfs = drawMainScreen(); //Display pure text GUI on generic background
			else if (!setting->Show_Menu) { //Display only GUI jpg
				setLaunchKeys();
				clrScr(setting->color[0]);
			}
			else //Display launch filenames/titles on GUI jpg
				nElfs = drawMainScreen2(TV_mode);
		}
		drawScr();
		post_event = event;
		event = 0;

		//Pad response section
		if(!init_delay && (waitAnyPadReady(), readpad())){
			if(new_pad){
				event |= 2;  //event |= pad command
			}
			RunELF_index = -1;
			switch(mode){
			case BUTTON:
				if(new_pad & PAD_CIRCLE)        RunELF_index = 1;
				else if(new_pad & PAD_CROSS)    RunELF_index = 2;
				else if(new_pad & PAD_SQUARE)   RunELF_index = 3;
				else if(new_pad & PAD_TRIANGLE) RunELF_index = 4;
				else if(new_pad & PAD_L1)       RunELF_index = 5;
				else if(new_pad & PAD_R1)       RunELF_index = 6;
				else if(new_pad & PAD_L2)       RunELF_index = 7;
				else if(new_pad & PAD_R2)       RunELF_index = 8;
				else if(new_pad & PAD_L3)       RunELF_index = 9;
				else if(new_pad & PAD_R3)       RunELF_index = 10;
				else if(new_pad & PAD_START)    RunELF_index = 11;
				else if(new_pad & PAD_SELECT)   RunELF_index = 12;
				else if((new_pad & PAD_LEFT) && (maxCNF > 1 || setting->LK_Flag[i]))
					RunELF_index = 13;
				else if((new_pad & PAD_RIGHT) && (maxCNF > 1 || setting->LK_Flag[i]))
					RunELF_index = 14;
				else if(new_pad & PAD_UP || new_pad & PAD_DOWN){
					user_acted = 1;
					if (!setting->Show_Menu && setting->GUI_skin[0]){} //GUI Menu: disabled when there's no text on menu screen
					else{
						selected=0;
						mode=DPAD;
					}
				}
				if(RunELF_index >= 0 && setting->LK_Path[RunELF_index][0]){
					user_acted = 1;
					RunElf(setting->LK_Path[RunELF_index]);
				}
				break;
			
			case DPAD:
				if(new_pad & PAD_UP){
					selected--;
					if(selected<0)
						selected=nElfs-1;
				}else if(new_pad & PAD_DOWN){
					selected++;
					if(selected>=nElfs)
						selected=0;
				}else if((!swapKeys && new_pad & PAD_CROSS)
				      || (swapKeys && new_pad & PAD_CIRCLE) ){
					mode=BUTTON;
				}else if((swapKeys && new_pad & PAD_CROSS)
				      || (!swapKeys && new_pad & PAD_CIRCLE) ){
					if(setting->LK_Path[menu_LK[selected]][0])
						RunElf(setting->LK_Path[menu_LK[selected]]);
					mode=BUTTON;
				}
				break;
			}
		}//ends Pad response section

		if(timeout/SCANRATE==0 && setting->LK_Path[0][0] && mode==BUTTON && !user_acted){
			RunElf(setting->LK_Path[0]);
			user_acted = 1; //Halt timeout after default action, just as for user action
			event |= 8;  //event |= visible timeout change
		}
	}
	//----- End of main menu event loop -----
}
//------------------------------
//endfunc main
//--------------------------------------------------------------
//End of file: main.c
//--------------------------------------------------------------
