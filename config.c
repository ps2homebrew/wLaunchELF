//---------------------------------------------------------------------------
// File name:   config.c
//---------------------------------------------------------------------------
#include "launchelf.h"
#include <stdbool.h>

enum
{
	DEF_TIMEOUT = 10,
	DEF_HIDE_PATHS = TRUE,
	DEF_COLOR1 = ITO_RGBA(128,128,128,0), //Backgr
	DEF_COLOR2 = ITO_RGBA(64,64,64,0),    //Frame
	DEF_COLOR3 = ITO_RGBA(96,0,0,0),      //Select
	DEF_COLOR4 = ITO_RGBA(0,0,0,0),       //Text
	DEF_COLOR5 = ITO_RGBA(255,0,255,0),   //Graph1
	DEF_COLOR6 = ITO_RGBA(0,0,255,0),     //Graph2
	DEF_COLOR7 = ITO_RGBA(0,0,0,0),       //Graph3
	DEF_COLOR8 = ITO_RGBA(0,0,0,0),       //Graph4
	DEF_DISCCONTROL = FALSE,
	DEF_INTERLACE = FALSE,
	DEF_MENU_FRAME = TRUE,
	DEF_RESETIOP = TRUE,
	DEF_NUMCNF = 1,
	DEF_SWAPKEYS = FALSE,
	DEF_HOSTWRITE = FALSE,
	DEF_BRIGHT = 50,
	DEF_POPUP_OPAQUE = FALSE,
	DEF_INIT_DELAY = 0,
	DEF_USBKBD_USED = 1,
	DEF_SHOW_TITLES = 1,
	
	DEFAULT=0,
	SHOW_TITLES=12,
	DISCCONTROL,
	FILENAME,
	SCREEN,
	SETTINGS,
	NETWORK,
	OK,
	CANCEL
};

char LK_ID[15][10]={
	"auto",
	"Circle",
	"Cross",
	"Square",
	"Triangle",
	"L1",
	"R1",
	"L2",
	"R2",
	"L3",
	"R3",
	"Start",
	"Select",  //Predefined for "CONFIG"
	"Left",    //Predefined for "LOAD CONFIG--"
	"Right"    //Predefined for "LOAD CONFIG++"
};
char PathPad[30][MAX_PATH];
SETTING *setting = NULL;
SETTING *tmpsetting;
//---------------------------------------------------------------------------
// End of declarations
// Start of functions
//---------------------------------------------------------------------------
// get_CNF_string is the main CNF parser called for each CNF variable in a
// CNF file. Input and output data is handled via its pointer parameters.
// The return value flags 'false' when no variable is found. (normal at EOF)
//---------------------------------------------------------------------------
int	get_CNF_string(unsigned char **CNF_p_p,
                   unsigned char **name_p_p,
                   unsigned char **value_p_p)
{
	unsigned char *np, *vp, *tp = *CNF_p_p;

start_line:
	while((*tp<=' ') && (*tp>'\0')) tp+=1;  //Skip leading whitespace, if any
	if(*tp=='\0')	return false;            //but exit at EOF
	np = tp;                               //Current pos is potential name
	if(*tp<'A')                            //but may be a comment line
	{                                      //We must skip a comment line
		while((*tp!='\r')&&(*tp!='\n')&&(tp!='\0')) tp+=1;  //Seek line end
		goto start_line;                     //Go back to try next line
	}

	while((*tp>='A')||((*tp>='0')&&(*tp<='9'))) tp+=1;  //Seek name end
	if(*tp=='\0')	return false;            //but exit at EOF
	*tp++ = '\0';                          //terminate name string (passing)
	while((*tp<=' ') && (*tp>'\0')) tp+=1; //Skip post-name whitespace, if any
	if(*tp!='=') return false;             //exit (syntax error) if '=' missing
	tp += 1;                               //skip '='
	while((*tp<=' ') && (*tp>'\0')         //Skip pre-value whitespace, if any
		&& (*tp!='\r') && (*tp!='\n'))tp+=1; //but do not pass the end of the line
	if(*tp=='\0')	return false;            //but exit at EOF
	vp = tp;                               //Current pos is potential value

	while((*tp!='\r')&&(*tp!='\n')&&(tp!='\0')) tp+=1;  //Seek line end
	if(*tp!='\0') *tp++ = '\0';            //terminate value (passing if not EOF)
	while((*tp<=' ') && (*tp>'\0')) tp+=1;  //Skip following whitespace, if any

	*CNF_p_p = tp;                         //return new CNF file position
	*name_p_p = np;                        //return found variable name
	*value_p_p = vp;                       //return found variable value
	return true;                           //return control to caller
}	//Ends get_CNF_string

//---------------------------------------------------------------------------
int CheckMC(void)
{
	int dummy, ret;
	
	mcGetInfo(0, 0, &dummy, &dummy, &dummy);
	mcSync(0, NULL, &ret);

	if( -1 == ret || 0 == ret) return 0;

	mcGetInfo(1, 0, &dummy, &dummy, &dummy);
	mcSync(0, NULL, &ret);

	if( -1 == ret || 0 == ret ) return 1;

	return -11;
}
//---------------------------------------------------------------------------
// Save LAUNCHELF.CNF (or LAUNCHELFx.CNF with multiple pages)
// sincro: ADD save USBD_FILE string
// polo: ADD save SKIN_FILE string
//---------------------------------------------------------------------------
void saveConfig(char *mainMsg, char *CNF)
{
	int i, ret, fd;
	char c[MAX_PATH], tmp[26*MAX_PATH + 30*MAX_PATH];
	char cnf_path[MAX_PATH];
	size_t CNF_size, CNF_step;

	sprintf(tmp, "CNF_version = 2.0\r\n%n", &CNF_size); //Start CNF with version header

	for(i=0; i<12; i++){	//Loop to save the ELF paths for each launch key
		sprintf(tmp+CNF_size,
			"LK_%s_E1 = %s\r\n"
			"%n",           // %n causes NO output, but only a measurement
			LK_ID[i], setting->LK_Path[i],
			&CNF_step       // This variable measures the size of sprintf data
	  );
		CNF_size += CNF_step;
	}//ends for

	sprintf(tmp+CNF_size,
		"LK_auto_Timer = %d\r\n"
		"Menu_Hide_Paths = %d\r\n"
		"GUI_Col_1_ABGR = %08lX\r\n"
		"GUI_Col_2_ABGR = %08lX\r\n"
		"GUI_Col_3_ABGR = %08lX\r\n"
		"GUI_Col_4_ABGR = %08lX\r\n"
		"GUI_Col_5_ABGR = %08lX\r\n"
		"GUI_Col_6_ABGR = %08lX\r\n"
		"GUI_Col_7_ABGR = %08lX\r\n"
		"GUI_Col_8_ABGR = %08lX\r\n"
		"Screen_X = %d\r\n"
		"Screen_Y = %d\r\n"
		"Init_CDVD_Check = %d\r\n"
		"Screen_Interlace = %d\r\n"
		"Init_Reset_IOP = %d\r\n"
		"Menu_Pages = %d\r\n"
		"GUI_Swap_Keys = %d\r\n"
		"USBD_FILE = %s\r\n"
		"NET_HOSTwrite = %d\r\n"
		"SKIN_FILE = %s\r\n"
		"Menu_Title = %s\r\n"
		"Menu_Frame = %d\r\n"
		"SKIN_Brightness = %d\r\n"
		"TV_mode = %d\r\n"
		"Popup_Opaque = %d\r\n"
		"Init_Delay = %d\r\n"
		"USBKBD_USED = %d\r\n"
		"USBKBD_FILE = %s\r\n"
		"KBDMAP_FILE = %s\r\n"
		"Menu_Show_Titles = %d\r\n"
		"CNF_Path = %s\r\n"
		"%n",           // %n causes NO output, but only a measurement
		setting->timeout,    //auto_Timer
		setting->Hide_Paths,   //Menu_Hide_Paths
		setting->color[0],   //Col_1
		setting->color[1],   //Col_2
		setting->color[2],   //Col_3
		setting->color[3],   //Col_4
		setting->color[4],   //Col_5
		setting->color[5],   //Col_6
		setting->color[6],   //Col_7
		setting->color[7],   //Col_8
		setting->screen_x,   //Screen_X
		setting->screen_y,   //Screen_Y
		setting->discControl,  //Init_CDVD_Check
		setting->interlace,  //Screen_Interlace
		setting->resetIOP,   //Init_Reset_IOP
		setting->numCNF,     //Menu_Pages
		setting->swapKeys,   //GUI_Swap_Keys
		setting->usbd_file,  //USBD_FILE
		setting->HOSTwrite,  //NET_HOST_write
		setting->skin,       //SKIN_FILE
		setting->Menu_Title, //Menu_Title
		setting->Menu_Frame, //Menu_Frame
		setting->Brightness, //SKIN_Brightness
		setting->TV_mode,    //TV_mode
		setting->Popup_Opaque, //Popup_Opaque
		setting->Init_Delay,   //Init_Delay
		setting->usbkbd_used,  //USBKBD_USED
		setting->usbkbd_file,  //USBKBD_FILE
		setting->kbdmap_file,  //KBDMAP_FILE
		setting->Show_Titles,  //Menu_Show_Titles
		setting->CNF_Path,     //CNF_Path
		&CNF_step       // This variable measures the size of sprintf data
  );
	CNF_size += CNF_step;

	for(i=0; i<15; i++){  //Loop to save user defined launch key titles
		if(setting->LK_Title[i][0]){  //Only save non-empty strings
			sprintf(tmp+CNF_size,
				"LK_%s_Title = %s\r\n"
				"%n",           // %n causes NO output, but only a measurement
				LK_ID[i], setting->LK_Title[i],
				&CNF_step       // This variable measures the size of sprintf data
		  );
			CNF_size += CNF_step;
		}//ends if
	}//ends for

	for(i=0; i<30; i++){  //Loop to save non-empty PathPad entries
		if(PathPad[i][0]){  //Only save non-empty strings
			sprintf(tmp+CNF_size,
				"PathPad[%02d] = %s\r\n"
				"%n",           // %n causes NO output, but only a measurement
				i, PathPad[i],
				&CNF_step       // This variable measures the size of sprintf data
		  );
			CNF_size += CNF_step;
		}//ends if
	}//ends for

	strcpy(c, LaunchElfDir);
	strcat(c, CNF);
	ret = genFixPath(c, cnf_path);
	if((ret >= 0) && ((fd=genOpen(cnf_path, O_RDONLY)) >= 0))
		genClose(fd);
	else {  //Start of clause for failure to use LaunchElfDir
		if(setting->CNF_Path[0]==0) { //if NO CNF Path override defined
			if(!strncmp(LaunchElfDir, "mc", 2))
				sprintf(c, "mc%d:/SYS-CONF", LaunchElfDir[2]-'0');
			else
				sprintf(c, "mc%d:/SYS-CONF", CheckMC());
	
			if((fd=fioDopen(c)) >= 0){
				fioDclose(fd);
				char strtmp[MAX_PATH] = "/";
				strcat(c, strcat(strtmp, CNF));
			}else{
				strcpy(c, LaunchElfDir);
				strcat(c, CNF);
			}
		}
	}  //End of clause for failure to use LaunchElfDir

	ret = genFixPath(c, cnf_path);
	if((ret < 0) || ((fd=genOpen(cnf_path,O_CREAT|O_WRONLY|O_TRUNC)) < 0)){
		sprintf(c, "mc%d:/SYS-CONF", CheckMC());
		if((fd=fioDopen(c)) >= 0){
			fioDclose(fd);
			char strtmp[MAX_PATH] = "/";
			strcat(c, strcat(strtmp, CNF));
		}else{
			strcpy(c, LaunchElfDir);
			strcat(c, CNF);
		}
		ret = genFixPath(c, cnf_path);
		if((fd=genOpen(cnf_path,O_CREAT|O_WRONLY|O_TRUNC)) < 0){
			sprintf(mainMsg, "Failed To Save %s", CNF);
			return;
		}
	}
	ret = genWrite(fd,&tmp,CNF_size);
	if(ret==CNF_size)
		sprintf(mainMsg, "Saved Config (%s)", c);
	else
		sprintf(mainMsg, "Failed writing %s", CNF);
	genClose(fd);
}
//---------------------------------------------------------------------------
unsigned long hextoul(char *string)
{
	unsigned long value;
	char c;

	value = 0;
	while( !(((c=*string++)<'0')||(c>'F')||((c>'9')&&(c<'A'))) )
		value = value*16 + ((c>'9') ? (c-'A'+10) : (c-'0'));
	return value;
}
//---------------------------------------------------------------------------
// Load LAUNCHELF.CNF (or LAUNCHELFx.CNF with multiple pages)
// sincro: ADD load USBD_FILE string
// polo: ADD load SKIN_FILE string
//---------------------------------------------------------------------------
void loadConfig(char *mainMsg, char *CNF)
{
	int i, fd, tst, mcport, var_cnt, CNF_version;
	size_t CNF_size;
	char path[MAX_PATH];
	char cnf_path[MAX_PATH];
	unsigned char *RAM_p, *CNF_p, *name, *value;
	
	if(setting!=NULL)
		free(setting);
	setting = (SETTING*)malloc(sizeof(SETTING));

	for(i=0; i<12; i++) setting->LK_Path[i][0] = 0;
	for(i=0; i<15; i++) setting->LK_Title[i][0] = 0;
	for(i=0; i<30; i++) PathPad[i][0] = 0;

	strcpy(setting->LK_Path[1], "MISC/FileBrowser");
	setting->usbd_file[0] = '\0';
	setting->usbkbd_file[0] = '\0';
	setting->kbdmap_file[0] = '\0';
	setting->skin[0] = '\0';
	setting->Menu_Title[0] = '\0';
	setting->CNF_Path[0] = '\0';
	setting->timeout = DEF_TIMEOUT;
	setting->Hide_Paths = DEF_HIDE_PATHS;
	setting->color[0] = DEF_COLOR1;
	setting->color[1] = DEF_COLOR2;
	setting->color[2] = DEF_COLOR3;
	setting->color[3] = DEF_COLOR4;
	setting->color[4] = DEF_COLOR5;
	setting->color[5] = DEF_COLOR6;
	setting->color[6] = DEF_COLOR7;
	setting->color[7] = DEF_COLOR8;
	setting->screen_x = SCREEN_X;
	setting->screen_y = SCREEN_Y;
	setting->discControl = DEF_DISCCONTROL;
	setting->interlace = DEF_INTERLACE;
	setting->Menu_Frame = DEF_MENU_FRAME;
	setting->resetIOP = DEF_RESETIOP;
	setting->numCNF = DEF_NUMCNF;
	setting->swapKeys = DEF_SWAPKEYS;
	setting->HOSTwrite = DEF_HOSTWRITE;
	setting->Brightness = DEF_BRIGHT;
	setting->TV_mode = TV_mode_AUTO; //0==Console_auto, 1==NTSC, 2==PAL
	setting->Popup_Opaque = DEF_POPUP_OPAQUE;
	setting->Init_Delay = DEF_INIT_DELAY;
	setting->usbkbd_used = DEF_USBKBD_USED;
	setting->Show_Titles = DEF_SHOW_TITLES;

	strcpy(path, LaunchElfDir);
	strcat(path, CNF);
	if(!strncmp(path, "cdrom", 5)) strcat(path, ";1");

	fd=-1;
	if((tst = genFixPath(path, cnf_path)) >= 0)
		fd = genOpen(cnf_path, O_RDONLY);
	if(fd<0) {
		char strtmp[MAX_PATH], *p;
		int  pos;

		p = strrchr(path, '.');
		if(*(p-1)!='F')
			p--;
		pos = (p-path);
		strcpy(strtmp, path);
		strcpy(strtmp+pos-9, "LNCHELF");
		strcpy(strtmp+pos-2, path+pos);
		if((tst = genFixPath(strtmp, cnf_path)) >= 0)
			fd = genOpen(cnf_path, O_RDONLY);
		if(fd<0) {
			if(!strncmp(LaunchElfDir, "mc", 2))
				mcport = LaunchElfDir[2]-'0';
			else
				mcport = CheckMC();
			if(mcport==1 || mcport==0){
				sprintf(strtmp, "mc%d:/SYS-CONF/", mcport);
				strcpy(path, strtmp);
				strcat(path, CNF);
				fd = genOpen(path, O_RDONLY);
				if(fd>=0)
					strcpy(LaunchElfDir, strtmp);
			}
		}
	}
	if(fd<0) {
failed_load:
		sprintf(mainMsg, "Failed To Load %s", CNF);
		return;
	}
	// This point is only reached after succefully opening CNF

	CNF_size = genLseek(fd, 0, SEEK_END);
	printf("CNF_size=%d\n", CNF_size);
	genLseek(fd, 0, SEEK_SET);
	RAM_p = (char*)malloc(CNF_size);
	CNF_p = RAM_p;
	if	(CNF_p==NULL)	{ genClose(fd); goto failed_load; }
		
	genRead(fd, CNF_p, CNF_size);  //Read CNF as one long string
	genClose(fd);
	CNF_p[CNF_size] = '\0';        //Terminate the CNF string

//RA NB: in the code below, the 'LK_' variables havee been implemented such that
//       any _Ex suffix will be accepted, with identical results. This will need
//       to be modified when more execution metods are implemented.

  CNF_version = 0;  // The CNF version is still unidentified
	for(var_cnt = 0; get_CNF_string(&CNF_p, &name, &value); var_cnt++)
	{	// A variable was found, now we dispose of its value.
		if(!strcmp(name,"CNF_version")) CNF_version = atoi(value);
		else if(CNF_version == 0) goto failed_load;  // Refuse unidentified CNF
		else if(!strncmp(name,"LK_auto_E",9))      strcpy(setting->LK_Path[0], value);
		else if(!strncmp(name,"LK_Circle_E",11))   strcpy(setting->LK_Path[1], value);
		else if(!strncmp(name,"LK_Cross_E",10))    strcpy(setting->LK_Path[2], value);
		else if(!strncmp(name,"LK_Square_E",11))   strcpy(setting->LK_Path[3], value);
		else if(!strncmp(name,"LK_Triangle_E",13)) strcpy(setting->LK_Path[4], value);
		else if(!strncmp(name,"LK_L1_E",7))        strcpy(setting->LK_Path[5], value);
		else if(!strncmp(name,"LK_R1_E",7))        strcpy(setting->LK_Path[6], value);
		else if(!strncmp(name,"LK_L2_E",7))        strcpy(setting->LK_Path[7], value);
		else if(!strncmp(name,"LK_R2_E",7))        strcpy(setting->LK_Path[8], value);
		else if(!strncmp(name,"LK_L3_E",7))        strcpy(setting->LK_Path[9], value);
		else if(!strncmp(name,"LK_R3_E",7))        strcpy(setting->LK_Path[10],value);
		else if(!strncmp(name,"LK_Start_E",10))    strcpy(setting->LK_Path[11],value);
		else if(!strcmp(name,"LK_auto_Timer")) setting->timeout = atoi(value);
		else if(!strcmp(name,"Menu_Hide_Paths")) setting->Hide_Paths = atoi(value);
		else if(!strcmp(name,"GUI_Col_1_ABGR")) setting->color[0] = hextoul(value);
		else if(!strcmp(name,"GUI_Col_2_ABGR")) setting->color[1] = hextoul(value);
		else if(!strcmp(name,"GUI_Col_3_ABGR")) setting->color[2] = hextoul(value);
		else if(!strcmp(name,"GUI_Col_4_ABGR")) setting->color[3] = hextoul(value);
		else if(!strcmp(name,"GUI_Col_5_ABGR")) setting->color[4] = hextoul(value);
		else if(!strcmp(name,"GUI_Col_6_ABGR")) setting->color[5] = hextoul(value);
		else if(!strcmp(name,"GUI_Col_7_ABGR")) setting->color[6] = hextoul(value);
		else if(!strcmp(name,"GUI_Col_8_ABGR")) setting->color[7] = hextoul(value);
		else if(!strcmp(name,"Screen_X")) setting->screen_x = atoi(value);
		else if(!strcmp(name,"Screen_Y")) setting->screen_y = atoi(value);
		else if(!strcmp(name,"Init_CDVD_Check")) setting->discControl = atoi(value);
		else if(!strcmp(name,"Screen_Interlace")) setting->interlace = atoi(value);
		else if(!strcmp(name,"Init_Reset_IOP")) setting->resetIOP = atoi(value);
		else if(!strcmp(name,"Menu_Pages")) setting->numCNF = atoi(value);
		else if(!strcmp(name,"GUI_Swap_Keys")) setting->swapKeys = atoi(value);
		else if(!strcmp(name,"USBD_FILE")) strcpy(setting->usbd_file,value);
		else if(!strcmp(name,"NET_HOSTwrite")) setting->HOSTwrite = atoi(value);
		else if(!strcmp(name,"SKIN_FILE")) strcpy(setting->skin,value);
		else if(!strcmp(name,"Menu_Title")){
			strncpy(setting->Menu_Title, value, MAX_MENU_TITLE);
			setting->Menu_Title[MAX_MENU_TITLE] = '\0';
		}
		else if(!strcmp(name,"Menu_Frame")) setting->Menu_Frame = atoi(value);
		else if(!strcmp(name,"SKIN_Brightness")) setting->Brightness = atoi(value);
		else if(!strcmp(name,"TV_mode")) setting->TV_mode = atoi(value);
		else if(!strcmp(name,"Popup_Opaque")) setting->Popup_Opaque = atoi(value);
		else if(!strcmp(name,"Init_Delay")) setting->Init_Delay = atoi(value);
		else if(!strcmp(name,"USBKBD_USED")) setting->usbkbd_used = atoi(value);
		else if(!strcmp(name,"USBKBD_FILE")) strcpy(setting->usbkbd_file,value);
		else if(!strcmp(name,"KBDMAP_FILE")) strcpy(setting->kbdmap_file,value);
		else if(!strcmp(name,"Menu_Show_Titles")) setting->Show_Titles = atoi(value);
		else if(!strcmp(name,"CNF_Path")) strcpy(setting->CNF_Path,value);
		else if(!strcmp(name,"LK_auto_Title"))     strncpy(setting->LK_Title[0], value, MAX_ELF_TITLE-1);
		else if(!strcmp(name,"LK_Circle_Title"))   strncpy(setting->LK_Title[1], value, MAX_ELF_TITLE-1);
		else if(!strcmp(name,"LK_Cross_Title"))    strncpy(setting->LK_Title[2], value, MAX_ELF_TITLE-1);
		else if(!strcmp(name,"LK_Square_Title"))   strncpy(setting->LK_Title[3], value, MAX_ELF_TITLE-1);
		else if(!strcmp(name,"LK_Triangle_Title")) strncpy(setting->LK_Title[4], value, MAX_ELF_TITLE-1);
		else if(!strcmp(name,"LK_L1_Title"))       strncpy(setting->LK_Title[5], value, MAX_ELF_TITLE-1);
		else if(!strcmp(name,"LK_R1_Title"))       strncpy(setting->LK_Title[6], value, MAX_ELF_TITLE-1);
		else if(!strcmp(name,"LK_L2_Title"))       strncpy(setting->LK_Title[7], value, MAX_ELF_TITLE-1);
		else if(!strcmp(name,"LK_R2_Title"))       strncpy(setting->LK_Title[8], value, MAX_ELF_TITLE-1);
		else if(!strcmp(name,"LK_L3_Title"))       strncpy(setting->LK_Title[9], value, MAX_ELF_TITLE-1);
		else if(!strcmp(name,"LK_R3_Title"))       strncpy(setting->LK_Title[10], value, MAX_ELF_TITLE-1);
		else if(!strcmp(name,"LK_Start_Title"))    strncpy(setting->LK_Title[11], value, MAX_ELF_TITLE-1);
		else if(!strcmp(name,"LK_Select_Title"))   strncpy(setting->LK_Title[12], value, MAX_ELF_TITLE-1);
		else if(!strcmp(name,"LK_Left_Title"))     strncpy(setting->LK_Title[13], value, MAX_ELF_TITLE-1);
		else if(!strcmp(name,"LK_Right_Title"))    strncpy(setting->LK_Title[14], value, MAX_ELF_TITLE-1);
		else if(!strncmp(name,"PathPad[",8)){
			i = atoi(name+8);
			if(i < 30){
				strncpy(PathPad[i], value, MAX_PATH-1);
				PathPad[i][MAX_PATH-1] = '\0';
			}
		}
	}
	for(i=0; i<15; i++) setting->LK_Title[i][MAX_ELF_TITLE-1] = 0;
	free(RAM_p);
	sprintf(mainMsg, "Loaded Config (%s)", path);
	return;
}
//---------------------------------------------------------------------------
// Polo: ADD Skin Menu with Skin preview
//---------------------------------------------------------------------------
void Config_Skin(void)
{
	int  s, max_s=4;
	int  x, y;
	int event, post_event=0;
	char c[MAX_PATH];
	char skinSave[MAX_PATH];
	int  Brightness = setting->Brightness;

	strcpy(skinSave, setting->skin);

	loadSkin(PREVIEW_PIC);

	s=1;
	event = 1;  //event = initial entry
	while(1)
	{
		//Pad response section
		waitPadReady(0, 0);
		if(readpad())
		{
			if(new_pad & PAD_UP)
			{
				event |= 2;  //event |= valid pad command
				if(s!=1) s--;
				else s=max_s;
			}
			else if(new_pad & PAD_DOWN)
			{
				event |= 2;  //event |= valid pad command
				if(s!=max_s) s++;
				else s=1;
			}
			else if(new_pad & PAD_LEFT)
			{
				event |= 2;  //event |= valid pad command
				if(s!=1) s=1;
				else s=max_s;
			}
			else if(new_pad & PAD_RIGHT)
			{
				event |= 2;  //event |= valid pad command
				if(s!=max_s) s=max_s;
				else s=1;
			}
			else if((!swapKeys && new_pad & PAD_CROSS) || (swapKeys && new_pad & PAD_CIRCLE) )
			{
				event |= 2;  //event |= valid pad command
				if(s==1) {
					setting->skin[0] = '\0';
					loadSkin(PREVIEW_PIC);
				} else if(s==3) {
					if((Brightness > 0)&&(testsetskin == 1)) {
						Brightness--;
					}
				}
			}
			else if((swapKeys && new_pad & PAD_CROSS) || (!swapKeys && new_pad & PAD_CIRCLE))
			{
				event |= 2;  //event |= valid pad command
				if(s==1) {
					getFilePath(setting->skin, SKIN_CNF);
					loadSkin(PREVIEW_PIC);
				} else if(s==2) {
					loadSkin(BACKGROUND_PIC);
					setting->Brightness = Brightness;
					return;
				} else if(s==3) {
					if((Brightness < 100)&&(testsetskin == 1)) {
						Brightness++;
					}
				} else if(s==4) {
					setting->skin[0] = '\0';
					strcpy(setting->skin, skinSave);
					return;
				}
			}
			else if(new_pad & PAD_TRIANGLE) {
				setting->skin[0] = '\0';
				strcpy(setting->skin, skinSave);
				return;
			}
		} //end if(readpad())
		
		if(event||post_event){ //NB: We need to update two frame buffers per event

			//Display section
			clrScr(setting->color[0]);

			drawFrame( ( SCREEN_WIDTH/4 )-2, ( ( ( SCREEN_HEIGHT/2 )/4 )+10 )-1,
			 ( ( SCREEN_WIDTH/4 )*3 )+1, ( ( ( SCREEN_HEIGHT/2 )/4 )*3 )+10,
			  setting->color[1]);
			itoSetTexture(SCREEN_WIDTH*SCREEN_HEIGHT/2*4, SCREEN_WIDTH/2, ITO_RGB24, log(SCREEN_WIDTH/2), log(SCREEN_HEIGHT/4));
			if ( testsetskin == 1 ) {
				setBrightness(Brightness);
				itoTextureSprite(ITO_RGBAQ(0x80, 0x80, 0x80, 0xFF, 0),
				 SCREEN_WIDTH/4, ( ( SCREEN_HEIGHT/2 )/4 )+10,
				 0, 0,
				 ( SCREEN_WIDTH/4 )*3, ( ( ( SCREEN_HEIGHT/2 )/4 )*3 )+10,
				 SCREEN_WIDTH/2, SCREEN_HEIGHT/4,
				 0);
				setBrightness(50);
			}else{
				itoSprite(setting->color[0],
				 SCREEN_WIDTH/4, ( ( SCREEN_HEIGHT/2 )/4 )+10,
				 ( SCREEN_WIDTH/4 )*3, ( ( ( SCREEN_HEIGHT/2 )/4 )*3 )+10,
				 0);
			}

			x = Menu_start_x;
			y = Menu_start_y;
		
			printXY("SKIN SETTINGS", x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;
			y += FONT_HEIGHT/2;

			if(strlen(setting->skin)==0)
				sprintf(c, "  Skin Path: NULL");
			else
				sprintf(c, "  Skin Path: %s",setting->skin);
			printXY(c, x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;
			y += FONT_HEIGHT/2;

			printXY("  Apply New Skin", x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;
			y += FONT_HEIGHT/2;

			sprintf(c, "  Brightness: %d", Brightness);
			printXY(c, x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;
			y += FONT_HEIGHT / 2;

			printXY("  RETURN", x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;

			//Cursor positioning section
			y = Menu_start_y + s*FONT_HEIGHT + FONT_HEIGHT/2;

			if(s>=2) y+=FONT_HEIGHT/2;
			if(s>=3) y+=FONT_HEIGHT/2;
			if(s>=4) y+=FONT_HEIGHT/2;
			drawChar(127, x, y/2, setting->color[3]);

			//Tooltip section
			if (s == 1) {
				if (swapKeys)
					sprintf(c, "ÿ1:Edit ÿ0:Clear");
				else
					sprintf(c, "ÿ0:Edit ÿ1:Clear");
			} else if (s == 3) {  //if cursor at a colour component or a screen offset
				if (swapKeys)
					strcpy(c, "ÿ1:Add ÿ0:Subtract");
				else
					strcpy(c, "ÿ0:Add ÿ1:Subtract");
			} else {
				if (swapKeys)
					strcpy(c, "ÿ1:OK");
				else
					strcpy(c, "ÿ0:OK");
			}
			strcat(c, " ÿ3:Return");
			setScrTmp("", c);
		}//ends if(event||post_event)
		drawScr();
		post_event = event;
		event = 0;

	}//ends while
}//ends Config_Skin
//---------------------------------------------------------------------------
void Config_Screen(void)
{
	int i;
	int s, max_s=32;		//define cursor index and its max value
	int x, y;
	int event, post_event=0;
	uint64 rgb[8][3];
	char c[MAX_PATH];
	
	event = 1;	//event = initial entry

	for(i=0; i<8; i++) {
		rgb[i][0] = setting->color[i] & 0xFF;
		rgb[i][1] = setting->color[i] >> 8 & 0xFF;
		rgb[i][2] = setting->color[i] >> 16 & 0xFF;
	}
	
	s=0;
	while(1)
	{
		//Pad response section
		waitPadReady(0, 0);
		if(readpad())
		{
			if(new_pad & PAD_UP)
			{
				event |= 2;  //event |= valid pad command
				if(s==0)
					s=max_s;
				else if(s==24)
					s=2;
				else
					s--;
			}
			else if(new_pad & PAD_DOWN)
			{
				event |= 2;  //event |= valid pad command
				if((s<24)&&(s%3==2))
					s=24;
				else if(s==max_s)
					s=0;
				else
					s++;
			}
			else if(new_pad & PAD_LEFT)
			{
				event |= 2;  //event |= valid pad command
				if(s>=29) s=28;
				else if(s>=28) s=27;
				else if(s>=27) s=26;
				else if(s>=26) s=24;
				else if(s>=24) s=21; //if s beyond color settings
				else if(s>=3) s-=3;  //if s in a color beyond Color1 step to preceding color
			}
			else if(new_pad & PAD_RIGHT)
			{
				event |= 2;  //event |= valid pad command
				if(s>=28) s=29;
				else if(s>=27) s=28;
				else if(s>=26) s=27;
				else if(s>=24) s=26;
				else if(s>=21) s=24; //if s in Color8, move it to ScreenX
				else s+=3;           //if s in a color before Color8, step to next color
			}
			else if((!swapKeys && new_pad & PAD_CROSS) || (swapKeys && new_pad & PAD_CIRCLE) )
			{ //User pressed CANCEL=>Subtract/Clear
				event |= 2;  //event |= valid pad command
				if(s<24) {
					if(rgb[s/3][s%3] > 0) {
						rgb[s/3][s%3]--;
						setting->color[s/3] = 
							ITO_RGBA(rgb[s/3][0], rgb[s/3][1], rgb[s/3][2], 0);
					}
				} else if(s==24) {
					if(setting->screen_x > 0) {
						setting->screen_x--;
						screen_env.screen.x = setting->screen_x;
						itoSetScreenPos(setting->screen_x, setting->screen_y);
					}
				} else if(s==25) {
					if(setting->screen_y > 0) {
						setting->screen_y--;
						screen_env.screen.y = setting->screen_y;
						itoSetScreenPos(setting->screen_x, setting->screen_y);
					}
				} else if(s==28) {  //cursor is at Menu_Title
						setting->Menu_Title[0] = '\0';
				}
			}
			else if((swapKeys && new_pad & PAD_CROSS) || (!swapKeys && new_pad & PAD_CIRCLE))
			{ //User pressed OK=>Add/Ok/Edit
				event |= 2;  //event |= valid pad command
				if(s<24) {
					if(rgb[s/3][s%3] < 255) {
						rgb[s/3][s%3]++;
						setting->color[s/3] = 
							ITO_RGBA(rgb[s/3][0], rgb[s/3][1], rgb[s/3][2], 0);
					}
				} else if(s==24) {
					setting->screen_x++;
					screen_env.screen.x = setting->screen_x;
					itoSetScreenPos(setting->screen_x, setting->screen_y);
				} else if(s==25) {
					setting->screen_y++;
					screen_env.screen.y = setting->screen_y;
					itoSetScreenPos(setting->screen_x, setting->screen_y);
				} else if(s==26) {
					setting->interlace = !setting->interlace;
					screen_env.interlace = setting->interlace;
					itoGsReset();
					itoGsEnvSubmit(&screen_env);
					if(setting->interlace)
						setting->screen_y = (setting->screen_y-1)*2;
					else
						setting->screen_y = setting->screen_y/2+1;
					itoSetScreenPos(setting->screen_x, setting->screen_y);
				} else if(s==27) {
					Config_Skin();
				} else if(s==28) {  //cursor is at Menu_Title
					char tmp[MAX_MENU_TITLE+1];
					strcpy(tmp, setting->Menu_Title);
					if(keyboard(tmp, 36)>=0)
						strcpy(setting->Menu_Title, tmp);
				} else if(s==29) {
					setting->Menu_Frame = !setting->Menu_Frame;
				} else if(s==30) {
					setting->Popup_Opaque = !setting->Popup_Opaque;
				} else if(s==max_s-1) { //Always put 'RETURN' next to last
					return;
				} else if(s==max_s) { //Always put 'DEFAULT SCREEN SETTINGS' last
					setting->skin[0] = '\0';
					loadSkin(BACKGROUND_PIC);
					setting->color[0] = DEF_COLOR1;
					setting->color[1] = DEF_COLOR2;
					setting->color[2] = DEF_COLOR3;
					setting->color[3] = DEF_COLOR4;
					setting->color[4] = DEF_COLOR5;
					setting->color[5] = DEF_COLOR6;
					setting->color[6] = DEF_COLOR7;
					setting->color[7] = DEF_COLOR8;
					setting->screen_x = SCREEN_X;
					setting->screen_y = SCREEN_Y;
					setting->interlace = DEF_INTERLACE;
					setting->Menu_Frame = DEF_MENU_FRAME;
					setting->Brightness = DEF_BRIGHT;
					setting->Popup_Opaque = DEF_POPUP_OPAQUE;
					updateScreenMode();
					itoSetBgColor(GS_border_colour);
					
					for(i=0; i<8; i++) {
						rgb[i][0] = setting->color[i] & 0xFF;
						rgb[i][1] = setting->color[i] >> 8 & 0xFF;
						rgb[i][2] = setting->color[i] >> 16 & 0xFF;
					}
				}
			}
			else if(new_pad & PAD_TRIANGLE)
				return;
		}

		if(event||post_event){ //NB: We need to update two frame buffers per event

			//Display section
			clrScr(setting->color[0]);
			itoSetBgColor(GS_border_colour);

			x = Menu_start_x;
			y = Menu_start_y;

			sprintf(c, "    Color1  Color2  Color3  Color4  Color5  Color6  Color7  Color8");
			printXY(c, x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;
			sprintf(c, "    Backgr  Frames  Select  Normal  Graph1  Graph2  Graph3  Graph4");
			printXY(c, x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;
			sprintf(c, "R:    %02lX      %02lX      %02lX      %02lX"
			           "      %02lX      %02lX      %02lX      %02lX",
				rgb[0][0], rgb[1][0], rgb[2][0], rgb[3][0],
				rgb[4][0], rgb[5][0], rgb[6][0], rgb[7][0]);
			printXY(c, x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;
			sprintf(c, "G:    %02lX      %02lX      %02lX      %02lX"
			           "      %02lX      %02lX      %02lX      %02lX",
				rgb[0][1], rgb[1][1], rgb[2][1], rgb[3][1],
				rgb[4][1], rgb[5][1], rgb[6][1], rgb[7][1]);
			printXY(c, x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;
			sprintf(c, "B:    %02lX      %02lX      %02lX      %02lX"
			           "      %02lX      %02lX      %02lX      %02lX",
				rgb[0][2], rgb[1][2], rgb[2][2], rgb[3][2],
				rgb[4][2], rgb[5][2], rgb[6][2], rgb[7][2]);
			printXY(c, x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;
			sprintf(c, "ÿ4");
			for(i=0; i<8; i++)
				printXY(c, x+FONT_WIDTH*(6+i*8), y/2, setting->color[i], TRUE);
			y += FONT_HEIGHT;

			y += FONT_HEIGHT/2;

			sprintf(c, "  Screen X offset: %d", setting->screen_x);
			printXY(c, x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;
			sprintf(c, "  Screen Y offset: %d", setting->screen_y);
			printXY(c, x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;
			y += FONT_HEIGHT / 2;
		
			if(setting->interlace)
				sprintf(c, "  Interlace: ON");
			else
				sprintf(c, "  Interlace: OFF");
			printXY(c, x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;
			y += FONT_HEIGHT / 2;

			printXY("  Skin Settings...", x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;
			y += FONT_HEIGHT / 2;

			if(setting->Menu_Title[0]=='\0')
				sprintf(c, "  Menu Title: NULL");
			else
				sprintf(c, "  Menu Title: %s",setting->Menu_Title);
			printXY(c, x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;
			y += FONT_HEIGHT / 2;

			if(setting->Menu_Frame)
				sprintf(c, "  Menu Frame: ON");
			else
				sprintf(c, "  Menu Frame: OFF");
			printXY(c, x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;

			if(setting->Popup_Opaque)
				sprintf(c, "  Popups Opaque: ON");
			else
				sprintf(c, "  Popups Opaque: OFF");
			printXY(c, x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;
			y += FONT_HEIGHT / 2;

			printXY("  RETURN", x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;
			printXY("  Use Default Screen Settings", x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;

			//Cursor positioning section
			x = Menu_start_x;
			y = Menu_start_y;

			if(s<24){  //if cursor indicates a colour component
				int colnum = s/3;
				int comnum = s-colnum*3;
				x += (4+colnum*8)*FONT_WIDTH;
				y += (2+comnum)*FONT_HEIGHT;
			} else {  //if cursor indicates anything after colour components
				y += (s-24+6)*FONT_HEIGHT+FONT_HEIGHT/2;  //adjust y for cursor beyond colours
				//Here y is almost correct, except for additional group spacing
				if(s>=26)            //if cursor at or beyond interlace choice
					y+=FONT_HEIGHT/2;  //adjust for half-row space below screen offsets
				if(s>=27)            //if cursor at or beyond 'SKIN SETTINGS'
					y+=FONT_HEIGHT/2;  //adjust for half-row space below interlace choice
				if(s>=28)            //if cursor at or beyond 'Menu Title'
					y+=FONT_HEIGHT/2;  //adjust for half-row space below 'SKIN SETTINGS'
				if(s>=29)            //if cursor at or beyond 'Menu Frame'
					y+=FONT_HEIGHT/2;  //adjust for half-row space below 'Menu Title'
				if(s>=max_s-1)            //if cursor at or beyond 'RETURN'
					y+=FONT_HEIGHT/2;  //adjust for half-row space below 'Popups Opaque'
			}
			drawChar(127, x, y/2, setting->color[3]);  //draw cursor

			//Tooltip section
			if (s <= 25) {  //if cursor at a colour component or a screen offset
				if (swapKeys)
					strcpy(c, "ÿ1:Add ÿ0:Subtract");
				else
					strcpy(c, "ÿ0:Add ÿ1:Subtract");
			} else if(s==26||s==29||s==30) {
				//if cursor at 'INTERLACE' or 'Menu Frame' or 'Popups Opaque'
				if (swapKeys)
					strcpy(c, "ÿ1:Change");
				else
					strcpy(c, "ÿ0:Change");
			} else if(s==27){  //if cursor at 'SKIN SETTINGS'
				if (swapKeys)
					sprintf(c, "ÿ1:OK");
				else
					sprintf(c, "ÿ0:OK");
			} else if(s==28){  //if cursor at Menu_Title
				if (swapKeys)
					sprintf(c, "ÿ1:Edit ÿ0:Clear");
				else
					sprintf(c, "ÿ0:Edit ÿ1:Clear");
			} else {  //if cursor at 'RETURN' or 'DEFAULT' options
				if (swapKeys)
					strcpy(c, "ÿ1:OK");
				else
					strcpy(c, "ÿ0:OK");
			}
		
			strcat(c, " ÿ3:Return");
			setScrTmp("", c);
		}//ends if(event||post_event)
		drawScr();
		post_event = event;
		event = 0;

	}//ends while
}//ends Config_Screen
//---------------------------------------------------------------------------
// Other settings by EP
// sincro: ADD USBD SELECTOR MENU
// dlanor: Add Menu_Title config
//---------------------------------------------------------------------------
void Config_Startup(void)
{
	int s, max_s=12;		//define cursor index and its max value
	int x, y;
	int event, post_event=0;
	char c[MAX_PATH];

	event = 1;	//event = initial entry
	s=1;
	while(1)
	{
		//Pad response section
		waitPadReady(0, 0);
		if(readpad())
		{
			if(new_pad & PAD_UP)
			{
				event |= 2;  //event |= valid pad command
				if(s!=1) s--;
				else s=max_s;
			}
			else if(new_pad & PAD_DOWN)
			{
				event |= 2;  //event |= valid pad command
				if(s!=max_s) s++;
				else s=1;
			}
			else if(new_pad & PAD_LEFT)
			{
				event |= 2;  //event |= valid pad command
				if(s!=max_s) s=max_s;
				else s=1;
			}
			else if(new_pad & PAD_RIGHT)
			{
				event |= 2;  //event |= valid pad command
				if(s!=max_s) s=max_s;
				else s=1;
			}
			else if((!swapKeys && new_pad & PAD_CROSS) || (swapKeys && new_pad & PAD_CIRCLE) )
			{
				event |= 2;  //event |= valid pad command
				if(s==2 && setting->numCNF>1) setting->numCNF--;
				else if(s==4) setting->usbd_file[0] = '\0';
				else if(s==6 && setting->Init_Delay>0) setting->Init_Delay--;
				else if(s==7 && setting->timeout>0) setting->timeout--;
				else if(s==9) setting->usbkbd_file[0] = '\0';
				else if(s==10) setting->kbdmap_file[0] = '\0';
				else if(s==11) setting->CNF_Path[0] = '\0';
			}
			else if((swapKeys && new_pad & PAD_CROSS) || (!swapKeys && new_pad & PAD_CIRCLE))
			{
				event |= 2;  //event |= valid pad command
				if(s==1)
					setting->resetIOP = !setting->resetIOP;
				else if(s==2)
					setting->numCNF++;
				else if(s==3)
					setting->swapKeys = !setting->swapKeys;
				else if(s==4)
					getFilePath(setting->usbd_file, USBD_IRX_CNF);
				else if(s==5)
					setting->TV_mode = (setting->TV_mode+1)%3; //Change between 0,1,2
				else if(s==6)
					setting->Init_Delay++;
				else if(s==7)
					setting->timeout++;
				else if(s==8)
					setting->usbkbd_used = !setting->usbkbd_used;
				else if(s==9)
					getFilePath(setting->usbkbd_file, USBKBD_IRX_CNF);
				else if(s==10)
					getFilePath(setting->kbdmap_file, KBDMAP_FILE_CNF);
				else if(s==11)
				{	char *tmp;

					getFilePath(setting->CNF_Path, CNF_PATH_CNF);
					if((tmp = strrchr(setting->CNF_Path, '/')))
						tmp[1] = '\0';
				}
				else
					return;
			}
			else if(new_pad & PAD_TRIANGLE)
				return;
		}

		if(event||post_event){ //NB: We need to update two frame buffers per event

			//Display section
			clrScr(setting->color[0]);

			x = Menu_start_x;
			y = Menu_start_y;

			printXY("STARTUP SETTINGS", x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;
			y += FONT_HEIGHT / 2;

			if(setting->resetIOP)
				sprintf(c, "  Reset IOP: ON");
			else
				sprintf(c, "  Reset IOP: OFF");
			printXY(c, x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;

			sprintf(c, "  Number of CNF's: %d", setting->numCNF);
			printXY(c, x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;

			if(setting->swapKeys)
				sprintf(c, "  Pad mapping: ÿ1:OK ÿ0:CANCEL");
			else
				sprintf(c, "  Pad mapping: ÿ0:OK ÿ1:CANCEL");
			printXY(c, x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;

			if(strlen(setting->usbd_file)==0)
				sprintf(c, "  USBD IRX: DEFAULT");
			else
				sprintf(c, "  USBD IRX: %s",setting->usbd_file);
			printXY(c, x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;

			strcpy(c, "  TV mode: ");
			if(setting->TV_mode==TV_mode_NTSC)
				strcat(c, "NTSC");
			else if(setting->TV_mode==TV_mode_PAL)
				strcat(c, "PAL");
			else
				strcat(c, "AUTO");
			printXY(c, x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;

			sprintf(c, "  Initial Delay: %d", setting->Init_Delay);
			printXY(c, x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;

			sprintf(c, "  'Default' Timeout: %d", setting->timeout);
			printXY(c, x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;

			if(setting->usbkbd_used)
				sprintf(c, "  USB Keyboard Used: ON");
			else
				sprintf(c, "  USB Keyboard Used: OFF");
			printXY(c, x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;

			if(strlen(setting->usbkbd_file)==0)
				sprintf(c, "  USB Keyboard IRX: DEFAULT");
			else
				sprintf(c, "  USB Keyboard IRX: %s",setting->usbkbd_file);
			printXY(c, x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;

			if(strlen(setting->kbdmap_file)==0)
				sprintf(c, "  USB Keyboard Map: DEFAULT");
			else
				sprintf(c, "  USB Keyboard Map: %s",setting->kbdmap_file);
			printXY(c, x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;

			if(strlen(setting->CNF_Path)==0)
				sprintf(c, "  CNF Path override: NONE");
			else
				sprintf(c, "  CNF Path override: %s",setting->CNF_Path);
			printXY(c, x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;

			y += FONT_HEIGHT / 2;
			printXY("  RETURN", x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;

			//Cursor positioning section
			y = Menu_start_y + s*FONT_HEIGHT + FONT_HEIGHT /2;

			if(s>=max_s) y+=FONT_HEIGHT/2;
			drawChar(127, x, y/2, setting->color[3]);

			//Tooltip section
			if ((s==1)||(s==8)) { //resetIOP || usbkbd_used
				if (swapKeys)
					strcpy(c, "ÿ1:Change");
				else
					strcpy(c, "ÿ0:Change");
			} else if ((s==2)||(s==6)||(s==7)) { //numCNF || Init_Delay || timeout
				if (swapKeys)
					strcpy(c, "ÿ1:Add ÿ0:Subtract");
				else
					strcpy(c, "ÿ0:Add ÿ1:Subtract");
			} else if(s == 3) {
				if (swapKeys)
					sprintf(c, "ÿ1:Change");
				else
					sprintf(c, "ÿ0:Change");
			} else if((s==4)||(s==9)||(s==10)||(s==11)) {
			//usbd_file||usbkbd_file||kbdmap_file||CNF_Path
				if (swapKeys)
					sprintf(c, "ÿ1:Browse ÿ0:Clear");
				else
					sprintf(c, "ÿ0:Browse ÿ1:Clear");
			} else if(s == 5) {
				if (swapKeys)
					sprintf(c, "ÿ1:Change");
				else
					sprintf(c, "ÿ0:Change");
			} else {
				if (swapKeys)
					strcpy(c, "ÿ1:OK");
				else
					strcpy(c, "ÿ0:OK");
			}
		
			strcat(c, " ÿ3:Return");
			setScrTmp("", c);
		}//ends if(event||post_event)
		drawScr();
		post_event = event;
		event = 0;

	}//ends while
}//ends Config_Startup
//---------------------------------------------------------------------------
// Network settings GUI by Slam-Tilt
//---------------------------------------------------------------------------
void saveNetworkSettings(char *Message)
{
	char firstline[50];
	extern char ip[16];
	extern char netmask[16];
	extern char gw[16];
    int out_fd,in_fd;
    int ret=0,i=0;
	int size,sizeleft=0;
	char *ipconfigfile=0;

	// Default message, will get updated if save is sucessfull
	strcpy(Message,"Saved Failed");

	sprintf(firstline,"%s %s %s\n\r",ip,netmask,gw);


	mcSync(0,NULL,NULL);
	mcMkDir(0, 0, "SYS-CONF");
	mcSync(0, NULL, &ret);

	// This block looks at the existing ipconfig.dat and works out if there is
	// already any data beyond the first line. If there is it will get appended to the output
	// to new file later.

	in_fd = fioOpen("mc0:/SYS-CONF/IPCONFIG.DAT", O_RDONLY);
	if (in_fd >= 0) {

		size = fioLseek(in_fd, 0, SEEK_END);
		printf("size of existing file is %ibtyes\n\r",size);

		ipconfigfile = (char *)malloc(size);

		fioLseek(in_fd, 0, SEEK_SET);
		fioRead(in_fd, ipconfigfile, size);


		for (i=0; (ipconfigfile[i] != 0 && i<=size); i++)

		{
			// printf("%i-%c\n\r",i,ipconfigfile[i]);
		}

		sizeleft=size-i;

		fioClose(in_fd);
	}

	// Writing the data out

	out_fd=fioOpen("mc0:/SYS-CONF/IPCONFIG.DAT", O_WRONLY | O_TRUNC | O_CREAT);
	if(out_fd >=0)
	{
		mcSync(0, NULL, &ret);
		fioWrite(out_fd,firstline,strlen(firstline));
		mcSync(0, NULL, &ret);

		// If we have any extra data, spit that out too.
		if (sizeleft > 0)
		{
			mcSync(0, NULL, &ret);
			fioWrite(out_fd,&ipconfigfile[i],sizeleft);
			mcSync(0, NULL, &ret);
		}

		strcpy(Message,"Saved mc0:/SYS-CONF/IPCONFIG.DAT");

		fioClose(out_fd);

	}
}
//---------------------------------------------------------------------------
// Convert IP string to numbers
//---------------------------------------------------------------------------
void ipStringToOctet(char *ip, int ip_octet[4])
{

	// This takes a string (ip) representing an IP address and converts it
	// into an array of ints (ip_octet)
	// Rewritten 22/10/05

	char oct_str[5];
	int oct_cnt,i;

	oct_cnt = 0;
	oct_str[0]=0;

	for (i=0; ((i<=strlen(ip)) && (oct_cnt<4)); i++)
	{
		if ((ip[i] == '.') | (i==strlen(ip)))
		{
			ip_octet[oct_cnt] = atoi(oct_str);
			oct_cnt++;
			oct_str[0]=0;
		} else
			sprintf(oct_str,"%s%c",oct_str,ip[i]);
	}
}
//---------------------------------------------------------------------------
data_ip_struct BuildOctets(char *ip, char *nm, char *gw)
{

	// Populate 3 arrays with the ip address (as ints)

	data_ip_struct iplist;

	ipStringToOctet(ip,iplist.ip);
	ipStringToOctet(nm,iplist.nm);
	ipStringToOctet(gw,iplist.gw);

	return(iplist);
}
//---------------------------------------------------------------------------
void Config_Network(void)
{
	// Menu System for Network Settings Page.

	int s,l;
	int x, y;
	int event, post_event=0;
	char c[MAX_PATH];
	extern char ip[16];
	extern char netmask[16];
	extern char gw[16];
	data_ip_struct ipdata;
	char NetMsg[MAX_PATH] = "";

	event = 1;	//event = initial entry
	s=1;
	l=1;
	ipdata = BuildOctets(ip,netmask,gw);

	while(1)
	{
		//Pad response section
		waitPadReady(0, 0);
		if(readpad())
		{
			if(new_pad & PAD_UP)
			{
				event |= 2;  //event |= valid pad command
				if(s!=1) s--;
				else {s=5; l=1; }
			}
			else if(new_pad & PAD_DOWN)
			{
				event |= 2;  //event |= valid pad command
				if(s!=5) s++;
				else s=1;
				if(s>3) l=1;
			}
			else if(new_pad & PAD_LEFT)
			{
				event |= 2;  //event |= valid pad command
				if(s<4)
					if(l>1)
						l--;
			}
			else if(new_pad & PAD_RIGHT)
			{
				event |= 2;  //event |= valid pad command
				if(s<4)
					if(l<5)
						l++;
			}
			else if((!swapKeys && new_pad & PAD_CROSS) || (swapKeys && new_pad & PAD_CIRCLE) )
			{
				event |= 2;  //event |= valid pad command
				if((s<4) && (l>1))
				{
					if (s == 1)
					{
							if (ipdata.ip[l-2] > 0)
							{
								ipdata.ip[l-2]--;
							}
					}
					else if (s == 2)
					{
							if (ipdata.nm[l-2] > 0)
							{
								ipdata.nm[l-2]--;
							}
					}
					else if (s == 3)
					{
							if (ipdata.gw[l-2] > 0)
							{
								ipdata.gw[l-2]--;
							}
					}

				}
			}
			else if((swapKeys && new_pad & PAD_CROSS) || (!swapKeys && new_pad & PAD_CIRCLE))
			{
				event |= 2;  //event |= valid pad command
				if((s<4) && (l>1))
				{
					if (s == 1)
					{
							if (ipdata.ip[l-2] < 255)
							{
								ipdata.ip[l-2]++;
							}
					}
					else if (s == 2)
					{
							if (ipdata.nm[l-2] < 255)
							{
								ipdata.nm[l-2]++;
							}
					}
					else if (s == 3)
					{
							if (ipdata.gw[l-2] < 255)
							{
								ipdata.gw[l-2]++;
							}
					}

				}

				else if(s==4)
				{
					sprintf(ip,"%i.%i.%i.%i",ipdata.ip[0],ipdata.ip[1],ipdata.ip[2],ipdata.ip[3]);
					sprintf(netmask,"%i.%i.%i.%i",ipdata.nm[0],ipdata.nm[1],ipdata.nm[2],ipdata.nm[3]);
					sprintf(gw,"%i.%i.%i.%i",ipdata.gw[0],ipdata.gw[1],ipdata.gw[2],ipdata.gw[3]);

					saveNetworkSettings(NetMsg);
				}
				else
					return;
			}
			else if(new_pad & PAD_TRIANGLE)
				return;
		}

		if(event||post_event){ //NB: We need to update two frame buffers per event

			//Display section
			clrScr(setting->color[0]);

			x = Menu_start_x;
			y = Menu_start_y;

			printXY("NETWORK SETTINGS", x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;

			y += FONT_HEIGHT / 2;

			sprintf(c, "  IP Address:  %.3i . %.3i . %.3i . %.3i",ipdata.ip[0],ipdata.ip[1],ipdata.ip[2],ipdata.ip[3]);
			printXY(c, x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;

			sprintf(c, "  Netmask:     %.3i . %.3i . %.3i . %.3i",ipdata.nm[0],ipdata.nm[1],ipdata.nm[2],ipdata.nm[3]);
			printXY(c, x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;

			sprintf(c, "  Gateway:     %.3i . %.3i . %.3i . %.3i",ipdata.gw[0],ipdata.gw[1],ipdata.gw[2],ipdata.gw[3]);
			printXY(c, x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;

			y += FONT_HEIGHT / 2;

			sprintf(c, "  Save to \"mc0:/SYS-CONF/IPCONFIG.DAT\"");
			printXY(c, x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;

			y += FONT_HEIGHT / 2;
			printXY("  RETURN", x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;

			//Cursor positioning section
			y = Menu_start_y + s*FONT_HEIGHT + FONT_HEIGHT/2;

			if(s>=4) y+=FONT_HEIGHT/2;
			if(s>=5) y+=FONT_HEIGHT/2;
			if (l > 1)
				x += l*48 + 15;
			drawChar(127, x, y/2, setting->color[3]);

			//Tooltip section
			if ((s <4) && (l==1)) {
					strcpy(c, "Right D-Pad to Edit");
			} else if (s < 4) {
				if (swapKeys)
					strcpy(c, "ÿ1:Add ÿ0:Subtract");
				else
					strcpy(c, "ÿ0:Add ÿ1:Subtract");
			} else if( s== 4) {
				if (swapKeys)
					sprintf(c, "ÿ1:Save");
				else
					sprintf(c, "ÿ0:Save");
			} else {
				if (swapKeys)
					strcpy(c, "ÿ1:OK");
				else
					strcpy(c, "ÿ0:OK");
			}
	
			strcat(c, " ÿ3:Return");
			setScrTmp(NetMsg, c);
		}//ends if(event||post_event)
		drawScr();
		post_event = event;
		event = 0;

	}//ends while
}//ends Config_Network
//---------------------------------------------------------------------------
// Configuration menu
//---------------------------------------------------------------------------
void config(char *mainMsg, char *CNF)
{
	char c[MAX_PATH];
	char skinSave[MAX_PATH];
	char title_tmp[MAX_ELF_TITLE];
	char *localMsg;
	int i;
	int s;
	int x, y;
	int event, post_event=0;
	
	strcpy(skinSave, setting->skin);
	tmpsetting = setting;
	setting = (SETTING*)malloc(sizeof(SETTING));
	*setting = *tmpsetting;
	
	event = 1;	//event = initial entry
	s=0;
	while(1)
	{
		//Pad response section
		waitPadReady(0, 0);
		if(readpad())
		{
			if(new_pad & PAD_UP)
			{
				event |= 2;  //event |= valid pad command
				if(s!=0)
					s--;
				else
					s=CANCEL;
			}
			else if(new_pad & PAD_DOWN)
			{
				event |= 2;  //event |= valid pad command
				if(s!=CANCEL)
					s++;
				else
					s=0;
			}
			else if(new_pad & PAD_LEFT)
			{
				event |= 2;  //event |= valid pad command
				if(s>=OK)
					s=SHOW_TITLES;
				else
					s=DEFAULT;
			}
			else if(new_pad & PAD_RIGHT)
			{
				event |= 2;  //event |= valid pad command
				if(s<SHOW_TITLES)
					s=SHOW_TITLES;
				else if(s<OK)
					s=OK;
			}
			else if((new_pad & PAD_SQUARE) && (s<SHOW_TITLES)){
				event |= 2;  //event |= valid pad command
				strcpy(title_tmp, setting->LK_Title[s]);
				if(keyboard(title_tmp, MAX_ELF_TITLE)>=0)
					strcpy(setting->LK_Title[s], title_tmp);
			}
			else if((!swapKeys && new_pad & PAD_CROSS) || (swapKeys && new_pad & PAD_CIRCLE) )
			{
				event |= 2;  //event |= valid pad command
				if(s<SHOW_TITLES){
					setting->LK_Path[s][0]=0;
					setting->LK_Title[s][0]=0;
				}
			}
			else if((swapKeys && new_pad & PAD_CROSS) || (!swapKeys && new_pad & PAD_CIRCLE))
			{
				event |= 2;  //event |= valid pad command
				if(s<SHOW_TITLES)
				{
					getFilePath(setting->LK_Path[s], TRUE);
					if(!strncmp(setting->LK_Path[s], "mc0", 3) ||
						!strncmp(setting->LK_Path[s], "mc1", 3)){
						sprintf(c, "mc%s", &setting->LK_Path[s][3]);
						strcpy(setting->LK_Path[s], c);
					}
				}
				else if(s==SHOW_TITLES)
					setting->Show_Titles = !setting->Show_Titles;
				else if(s==FILENAME)
					setting->Hide_Paths = !setting->Hide_Paths;
				else if(s==DISCCONTROL)
					setting->discControl = !setting->discControl;
				else if(s==SCREEN)
					Config_Screen();
				else if(s==SETTINGS)
					Config_Startup();
				else if(s==NETWORK)
					Config_Network();
				else if(s==OK)
				{
					free(tmpsetting);
					saveConfig(mainMsg, CNF);
					break;
				}
				else if(s==CANCEL)
					goto cancel_exit;
			}
			else if(new_pad & PAD_TRIANGLE) {
cancel_exit:
				free(setting);
				setting = tmpsetting;
				updateScreenMode();
				strcpy(setting->skin, skinSave);
				loadSkin(BACKGROUND_PIC);
				mainMsg[0] = 0;
				break;
			}
		} //end if(readpad())
		
		if(event||post_event){ //NB: We need to update two frame buffers per event

			//Display section
			clrScr(setting->color[0]);

			if(s < SHOW_TITLES) localMsg = setting->LK_Title[s];
			else                localMsg = "";

			x = Menu_start_x;
			y = Menu_start_y;
			printXY("Button Settings", x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;
			for(i=0; i<12; i++)
			{
				switch(i)
				{
				case 0:
					strcpy(c,"  Default: ");
					break;
				case 1:
					strcpy(c,"  ÿ0     : ");
					break;
				case 2:
					strcpy(c,"  ÿ1     : ");
					break;
				case 3:
					strcpy(c,"  ÿ2     : ");
					break;
				case 4:
					strcpy(c,"  ÿ3     : ");
					break;
				case 5:
					strcpy(c,"  L1     : ");
					break;
				case 6:
					strcpy(c,"  R1     : ");
					break;
				case 7:
					strcpy(c,"  L2     : ");
					break;
				case 8:
					strcpy(c,"  R2     : ");
					break;
				case 9:
					strcpy(c,"  L3     : ");
					break;
				case 10:
					strcpy(c,"  R3     : ");
					break;
				case 11:
					strcpy(c,"  START  : ");
					break;
				}
				strcat(c, setting->LK_Path[i]);
				printXY(c, x, y/2, setting->color[3], TRUE);
				y += FONT_HEIGHT;
			}

			y += FONT_HEIGHT / 2;

			if(setting->Show_Titles)
				sprintf(c, "  Show launch titles: ON");
			else
				sprintf(c, "  Show launch titles: OFF");
			printXY(c, x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;

			if(setting->discControl)
				sprintf(c, "  Disc control: ON");
			else
				sprintf(c, "  Disc control: OFF");
			printXY(c, x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;

			if(setting->Hide_Paths)
				sprintf(c, "  Hide full ELF paths: ON");
			else
				sprintf(c, "  Hide full ELF paths: OFF");
			printXY(c, x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;

			printXY("  Screen Settings...", x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;
			printXY("  Startup Settings...", x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;
			printXY("  Network Settings...", x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;

			printXY("  OK", x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;
			printXY("  Cancel", x, y/2, setting->color[3], TRUE);

			//Cursor positioning section
			y = Menu_start_y + (s+1)*FONT_HEIGHT;
			if(s>=SHOW_TITLES)
				y += FONT_HEIGHT / 2;
			drawChar(127, x, y/2, setting->color[3]);

			//Tooltip section
			if (s < SHOW_TITLES) {
				if (swapKeys)
					sprintf(c, "ÿ1:Browse ÿ0:Clear ÿ2:Edit Title");
				else
					sprintf(c, "ÿ0:Browse ÿ1:Clear ÿ2:Edit Title");
			} else if((s==SHOW_TITLES)||(s==FILENAME)||(s==DISCCONTROL)) {
				if (swapKeys)
					sprintf(c, "ÿ1:Change");
				else
					sprintf(c, "ÿ0:Change");
			} else {
				if (swapKeys)
					sprintf(c, "ÿ1:OK");
				else
					sprintf(c, "ÿ0:OK");
			}

			strcat(c, " ÿ3:Return");
			setScrTmp(localMsg, c);
		}//ends if(event||post_event)
		drawScr();
		post_event = event;
		event = 0;

	}//ends while
}//ends config
//---------------------------------------------------------------------------
// End of file: config.c
//---------------------------------------------------------------------------
