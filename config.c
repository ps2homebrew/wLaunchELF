//---------------------------------------------------------------------------
// File name:   config.c
//---------------------------------------------------------------------------
#include "launchelf.h"
#include <stdbool.h>

enum
{
	DEF_TIMEOUT = 10,
	DEF_FILENAME = TRUE,
	DEF_COLOR1 = ITO_RGBA(128,128,128,0),
	DEF_COLOR2 = ITO_RGBA(64,64,64,0),
	DEF_COLOR3 = ITO_RGBA(96,0,0,0),
	DEF_COLOR4 = ITO_RGBA(0,0,0,0),
	DEF_DISCCONTROL = FALSE,
	DEF_INTERLACE = FALSE,
	DEF_MENU_FRAME = TRUE,
	DEF_RESETIOP = TRUE,
	DEF_NUMCNF = 1,
	DEF_SWAPKEYS = FALSE,
	DEF_HOSTWRITE = FALSE,
	DEF_BRIGHT = 50,
	
	DEFAULT=0,
	TIMEOUT=12,
	DISCCONTROL,
	FILENAME,
	SCREEN,
	SETTINGS,
	NETWORK,
	OK,
	CANCEL
};

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
	int ret, fd;
	char c[MAX_PATH], tmp[26*MAX_PATH];
	size_t CNF_size;

	sprintf(tmp,
		"CNF_version = 2.0\r\n"
		"LK_auto_E1 = %s\r\n"
		"LK_Circle_E1 = %s\r\n"
		"LK_Cross_E1 = %s\r\n"
		"LK_Square_E1 = %s\r\n"
		"LK_Triangle_E1 = %s\r\n"
		"LK_L1_E1 = %s\r\n"
		"LK_R1_E1 = %s\r\n"
		"LK_L2_E1 = %s\r\n"
		"LK_R2_E1 = %s\r\n"
		"LK_L3_E1 = %s\r\n"
		"LK_R3_E1 = %s\r\n"
		"LK_Start_E1 = %s\r\n"
		"LK_auto_Timer = %d\r\n"
		"Menu_Hide_Paths = %d\r\n"
		"GUI_Col_1_ABGR = %08lX\r\n"
		"GUI_Col_2_ABGR = %08lX\r\n"
		"GUI_Col_3_ABGR = %08lX\r\n"
		"GUI_Col_4_ABGR = %08lX\r\n"
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
		"%n",           // %n causes NO output, but only a measurement
		setting->dirElf[0],  //auto
		setting->dirElf[1],  //Circle
		setting->dirElf[2],  //Cross
		setting->dirElf[3],  //Square
		setting->dirElf[4],  //Triangle
		setting->dirElf[5],  //L1
		setting->dirElf[6],  //R1
		setting->dirElf[7],  //L2
		setting->dirElf[8],  //R2
		setting->dirElf[9],  //L3
		setting->dirElf[10], //R3
		setting->dirElf[11], //Start
		setting->timeout,    //auto_Timer
		setting->filename,   //Menu_Hide_Paths
		setting->color[0],   //Col_0
		setting->color[1],   //Col_1
		setting->color[2],   //Col_2
		setting->color[3],   //Col_3
		setting->screen_x,   //Screen_X
		setting->screen_y,   //Screen_Y
		setting->discControl,  //Init_CDVD_Check
		setting->interlace,  //Screen_Interlace
		setting->resetIOP,   //Init_Reset_IOP
		setting->numCNF,     //Menu_Pages
		setting->swapKeys,   //GUI_Swap_Keys
		setting->usbd,       //USBD_FILE
		setting->HOSTwrite,  //NET_HOST_write
		setting->skin,       //SKIN_FILE
		setting->Menu_Title, //Menu_Title
		setting->Menu_Frame, //Menu_Frame
		setting->Brightness, //SKIN_Brightness
		setting->TV_mode,    //TV_mode
		&CNF_size       // This variable measure the size of sprintf data
  );

	strcpy(c, LaunchElfDir);
	strcat(c, CNF);
	if((fd=fioOpen(c, O_RDONLY)) >= 0)
		fioClose(fd);
	else{
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

	if((fd=fioOpen(c,O_CREAT|O_WRONLY|O_TRUNC)) < 0){
		sprintf(mainMsg, "Failed To Save %s", CNF);
		return;
	}
	ret = fioWrite(fd,&tmp,CNF_size);
	if(ret==CNF_size)
		sprintf(mainMsg, "Saved Config (%s)", c);
	else
		sprintf(mainMsg, "Failed writing %s", CNF);
	fioClose(fd);
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
	int i, fd, mcport, var_cnt, CNF_version;
	size_t CNF_size;
	char path[MAX_PATH];
	unsigned char *RAM_p, *CNF_p, *name, *value;
	
	if(setting!=NULL)
		free(setting);
	setting = (SETTING*)malloc(sizeof(SETTING));

	for(i=0; i<12; i++)
		setting->dirElf[i][0] = '\0';
	strcpy(setting->dirElf[1], "MISC/FileBrowser");
	setting->usbd[0] = '\0';
	setting->skin[0] = '\0';
	setting->Menu_Title[0] = '\0';
	setting->timeout = DEF_TIMEOUT;
	setting->filename = DEF_FILENAME;
	setting->color[0] = DEF_COLOR1;
	setting->color[1] = DEF_COLOR2;
	setting->color[2] = DEF_COLOR3;
	setting->color[3] = DEF_COLOR4;
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
	strcpy(path, LaunchElfDir);
	strcat(path, CNF);
	if(!strncmp(path, "cdrom", 5)) strcat(path, ";1");
	fd = fioOpen(path, O_RDONLY);

	if(fd<0) {
		if(!strncmp(LaunchElfDir, "mc", 2))
			mcport = LaunchElfDir[2]-'0';
		else
			mcport = CheckMC();
		if(mcport==1 || mcport==0){
			char strtmp[MAX_PATH];
			sprintf(strtmp, "mc%d:/SYS-CONF/", mcport);
			strcpy(path, strtmp);
			strcat(path, CNF);
			fd = fioOpen(path, O_RDONLY);
			if(fd>=0)
				strcpy(LaunchElfDir, strtmp);
		}
	}

	if(fd<0) {
failed_load:
		sprintf(mainMsg, "Failed To Load %s", CNF);
		return;
	}
	// This point is only reached after succefully opening CNF

	CNF_size = fioLseek(fd, 0, SEEK_END);
	printf("CNF_size=%d\n", CNF_size);
	fioLseek(fd, 0, SEEK_SET);
	RAM_p = (char*)malloc(CNF_size);
	CNF_p = RAM_p;
	if	(CNF_p==NULL)	{ fioClose(fd); goto failed_load; }
		
	fioRead(fd, CNF_p, CNF_size);  //Read CNF as one long string
	fioClose(fd);
	CNF_p[CNF_size] = '\0';        //Terminate the CNF string

//RA NB: in the code below, the 'LK_' variables havee been implemented such that
//       any _Ex suffix will be accepted, with identical results. This will need
//       to be modified when more execution metods are implemented.

  CNF_version = 0;  // The CNF version is still unidentified
	for(var_cnt = 0; get_CNF_string(&CNF_p, &name, &value); var_cnt++)
	{	// A variable was found, now we dispose of its value.
		if(!strcmp(name,"CNF_version")) CNF_version = atoi(value);
		else if(CNF_version == 0) goto failed_load;  // Refuse unidentified CNF
		else if(!strncmp(name,"LK_auto_E",9))      strcpy(setting->dirElf[0], value);
		else if(!strncmp(name,"LK_Circle_E",11))   strcpy(setting->dirElf[1], value);
		else if(!strncmp(name,"LK_Cross_E",10))    strcpy(setting->dirElf[2], value);
		else if(!strncmp(name,"LK_Square_E",11))   strcpy(setting->dirElf[3], value);
		else if(!strncmp(name,"LK_Triangle_E",13)) strcpy(setting->dirElf[4], value);
		else if(!strncmp(name,"LK_L1_E",7))        strcpy(setting->dirElf[5], value);
		else if(!strncmp(name,"LK_R1_E",7))        strcpy(setting->dirElf[6], value);
		else if(!strncmp(name,"LK_L2_E",7))        strcpy(setting->dirElf[7], value);
		else if(!strncmp(name,"LK_R2_E",7))        strcpy(setting->dirElf[8], value);
		else if(!strncmp(name,"LK_L3_E",7))        strcpy(setting->dirElf[9], value);
		else if(!strncmp(name,"LK_R3_E",7))        strcpy(setting->dirElf[10],value);
		else if(!strncmp(name,"LK_Start_E",10))    strcpy(setting->dirElf[11],value);
		else if(!strcmp(name,"LK_auto_Timer")) setting->timeout = atoi(value);
		else if(!strcmp(name,"Menu_Hide_Paths")) setting->filename = atoi(value);
		else if(!strcmp(name,"GUI_Col_1_ABGR")) setting->color[0] = hextoul(value);
		else if(!strcmp(name,"GUI_Col_2_ABGR")) setting->color[1] = hextoul(value);
		else if(!strcmp(name,"GUI_Col_3_ABGR")) setting->color[2] = hextoul(value);
		else if(!strcmp(name,"GUI_Col_4_ABGR")) setting->color[3] = hextoul(value);
		else if(!strcmp(name,"Screen_X")) setting->screen_x = atoi(value);
		else if(!strcmp(name,"Screen_Y")) setting->screen_y = atoi(value);
		else if(!strcmp(name,"Init_CDVD_Check")) setting->discControl = atoi(value);
		else if(!strcmp(name,"Screen_Interlace")) setting->interlace = atoi(value);
		else if(!strcmp(name,"Init_Reset_IOP")) setting->resetIOP = atoi(value);
		else if(!strcmp(name,"Menu_Pages")) setting->numCNF = atoi(value);
		else if(!strcmp(name,"GUI_Swap_Keys")) setting->swapKeys = atoi(value);
		else if(!strcmp(name,"USBD_FILE")) strcpy(setting->usbd,value);
		else if(!strcmp(name,"NET_HOSTwrite")) setting->HOSTwrite = atoi(value);
		else if(!strcmp(name,"SKIN_FILE")) strcpy(setting->skin,value);
		else if(!strcmp(name,"Menu_Title")){
			strncpy(setting->Menu_Title, value, MAX_TITLE);
			setting->Menu_Title[MAX_TITLE] = '\0';
		}
		else if(!strcmp(name,"Menu_Frame")) setting->Menu_Frame = atoi(value);
		else if(!strcmp(name,"SKIN_Brightness")) setting->Brightness = atoi(value);
		else if(!strcmp(name,"TV_mode")) setting->TV_mode = atoi(value);
	}
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
		}
		
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
				sprintf(c, "  SKIN PATH: NULL");
			else
				sprintf(c, "  SKIN PATH: %s",setting->skin);
			printXY(c, x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;
			y += FONT_HEIGHT/2;

			printXY("  APPLY NEW SKIN", x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;
			y += FONT_HEIGHT/2;

			sprintf(c, "  BRIGHTNESS: %d", Brightness);
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
					sprintf(c, "Å~:Edit Åõ:Clear");
				else
					sprintf(c, "Åõ:Edit Å~:Clear");
			} else if (s == 3) {  //if cursor at a colour component or a screen offset
				if (swapKeys)
					strcpy(c, "Å~:Add Åõ:Subtract");
				else
					strcpy(c, "Åõ:Add Å~:Subtract");
			} else {
				if (swapKeys)
					strcpy(c, "Å~:OK");
				else
					strcpy(c, "Åõ:OK");
			}

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
	int s, max_s=19;		//define cursor index and its max value
	int x, y;
	int event, post_event=0;
	uint64 rgb[4][3];
	char c[MAX_PATH];
	
	event = 1;	//event = initial entry

	for(i=0; i<4; i++) {
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
				else if(s==12)
					s=2;
				else
					s--;
			}
			else if(new_pad & PAD_DOWN)
			{
				event |= 2;  //event |= valid pad command
				if((s<12)&&(s%3==2))
					s=12;
				else if(s==max_s)
					s=0;
				else
					s++;
			}
			else if(new_pad & PAD_LEFT)
			{
				event |= 2;  //event |= valid pad command
				if(s>=17) s=16;
				else if(s>=16) s=15;
				else if(s>=15) s=14;
				else if(s>=14) s=12;
				else if(s>=12) s=9;
				else if(s>=9) s-=3;  //if s in Color4, move it to Color3
				else if(s>=6) s-=3;  //if s in Color3, move it to Color2
				else if(s>=3) s-=3;  //if s in Color2, move it to Color1
			}
			else if(new_pad & PAD_RIGHT)
			{
				event |= 2;  //event |= valid pad command
				if(s>=16) s=17;
				else if(s>=15) s=16;
				else if(s>=14) s=15;
				else if(s>=12) s=14;
				else if(s>=9) s=12;  //if s in Color4, move it to ScreenX
				else if(s>=6) s+=3;  //if s in Color3, move it to Color4
				else if(s>=3) s+=3;  //if s in Color2, move it to Color3
				else s+=3;           //if s in Color1, move it to Color2
			}
			else if((!swapKeys && new_pad & PAD_CROSS) || (swapKeys && new_pad & PAD_CIRCLE) )
			{ //User pressed CANCEL=>Subtract/Clear
				event |= 2;  //event |= valid pad command
				if(s<12) {
					if(rgb[s/3][s%3] > 0) {
						rgb[s/3][s%3]--;
						setting->color[s/3] = 
							ITO_RGBA(rgb[s/3][0], rgb[s/3][1], rgb[s/3][2], 0);
					}
				} else if(s==12) {
					if(setting->screen_x > 0) {
						setting->screen_x--;
						screen_env.screen.x = setting->screen_x;
						itoSetScreenPos(setting->screen_x, setting->screen_y);
					}
				} else if(s==13) {
					if(setting->screen_y > 0) {
						setting->screen_y--;
						screen_env.screen.y = setting->screen_y;
						itoSetScreenPos(setting->screen_x, setting->screen_y);
					}
				} else if(s==16) {  //cursor is at Menu_Title
						setting->Menu_Title[0] = '\0';
				}
			}
			else if((swapKeys && new_pad & PAD_CROSS) || (!swapKeys && new_pad & PAD_CIRCLE))
			{ //User pressed OK=>Add/Ok/Edit
				event |= 2;  //event |= valid pad command
				if(s<12) {
					if(rgb[s/3][s%3] < 255) {
						rgb[s/3][s%3]++;
						setting->color[s/3] = 
							ITO_RGBA(rgb[s/3][0], rgb[s/3][1], rgb[s/3][2], 0);
					}
				} else if(s==12) {
					setting->screen_x++;
					screen_env.screen.x = setting->screen_x;
					itoSetScreenPos(setting->screen_x, setting->screen_y);
				} else if(s==13) {
					setting->screen_y++;
					screen_env.screen.y = setting->screen_y;
					itoSetScreenPos(setting->screen_x, setting->screen_y);
				} else if(s==14) {
					setting->interlace = !setting->interlace;
					screen_env.interlace = setting->interlace;
					itoGsReset();
					itoGsEnvSubmit(&screen_env);
					if(setting->interlace)
						setting->screen_y = (setting->screen_y-1)*2;
					else
						setting->screen_y = setting->screen_y/2+1;
					itoSetScreenPos(setting->screen_x, setting->screen_y);
				} else if(s==15) {
					Config_Skin();
				} else if(s==16) {  //cursor is at Menu_Title
					char tmp[MAX_TITLE+1];
					strcpy(tmp, setting->Menu_Title);
					if(keyboard(tmp, 36)>=0)
						strcpy(setting->Menu_Title, tmp);
				} else if(s==17) {
					setting->Menu_Frame = !setting->Menu_Frame;
				} else if(s==max_s-1) { //Always put 'RETURN' next to last
					return;
				} else if(s==max_s) { //Always put 'DEFAULT SCREEN SETTINGS' last
					setting->skin[0] = '\0';
					loadSkin(BACKGROUND_PIC);
					setting->color[0] = DEF_COLOR1;
					setting->color[1] = DEF_COLOR2;
					setting->color[2] = DEF_COLOR3;
					setting->color[3] = DEF_COLOR4;
					setting->screen_x = SCREEN_X;
					setting->screen_y = SCREEN_Y;
					setting->interlace = DEF_INTERLACE;
					setting->Menu_Frame = DEF_MENU_FRAME;
					updateScreenMode();
					itoSetBgColor(setting->color[0]);
					
					for(i=0; i<4; i++) {
						rgb[i][0] = setting->color[i] & 0xFF;
						rgb[i][1] = setting->color[i] >> 8 & 0xFF;
						rgb[i][2] = setting->color[i] >> 16 & 0xFF;
					}
				}
			}
		}

		if(event||post_event){ //NB: We need to update two frame buffers per event

			//Display section
			clrScr(setting->color[0]);
			itoSetBgColor(setting->color[0]);

			x = Menu_start_x;
			y = Menu_start_y;

			sprintf(c, "    Color1  Color2  Color3  Color4");
			printXY(c, x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;
			sprintf(c, "    Backgr  Frames  Select  Normal");
			printXY(c, x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;
			sprintf(c, "R:    %02lX      %02lX      %02lX      %02lX",
				rgb[0][0], rgb[1][0], rgb[2][0], rgb[3][0]);
			printXY(c, x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;
			sprintf(c, "G:    %02lX      %02lX      %02lX      %02lX",
				rgb[0][1], rgb[1][1], rgb[2][1], rgb[3][1]);
			printXY(c, x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;
			sprintf(c, "B:    %02lX      %02lX      %02lX      %02lX",
				rgb[0][2], rgb[1][2], rgb[2][2], rgb[3][2]);
			printXY(c, x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;
			sprintf(c, "Å°");
			for(i=0; i<4; i++)
				printXY(c, x+FONT_WIDTH*(6+i*8), y/2, setting->color[i], TRUE);
			y += FONT_HEIGHT;

			y += FONT_HEIGHT/2;

			sprintf(c, "  SCREEN X: %d", setting->screen_x);
			printXY(c, x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;
			sprintf(c, "  SCREEN Y: %d", setting->screen_y);
			printXY(c, x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;
			y += FONT_HEIGHT / 2;
		
			if(setting->interlace)
				sprintf(c, "  INTERLACE: ON");
			else
				sprintf(c, "  INTERLACE: OFF");
			printXY(c, x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;
			y += FONT_HEIGHT / 2;

			printXY("  SKIN SETTINGS", x, y/2, setting->color[3], TRUE);
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
			y += FONT_HEIGHT / 2;

			printXY("  RETURN", x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;
			printXY("  DEFAULT SCREEN SETTINGS", x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;

			//Cursor positioning section
			x = Menu_start_x;
			y = Menu_start_y;

			if(s<12){  //if cursor indicates a colour component
				int colnum = s/3;
				int comnum = s-colnum*3;
				x += (4+colnum*8)*FONT_WIDTH;
				y += (2+comnum)*FONT_HEIGHT;
			} else {  //if cursor indicates anything after colour components
				y += (s-12+6)*FONT_HEIGHT+FONT_HEIGHT/2;  //adjust y for cursor beyond colours
				//Here y is almost correct, except for additional group spacing
				if(s>=14)            //if cursor at or beyond interlace choice
					y+=FONT_HEIGHT/2;  //adjust for half-row space below screen offsets
				if(s>=15)            //if cursor at or beyond 'SKIN SETTINGS'
					y+=FONT_HEIGHT/2;  //adjust for half-row space below interlace choice
				if(s>=16)            //if cursor at or beyond 'Menu Title'
					y+=FONT_HEIGHT/2;  //adjust for half-row space below 'SKIN SETTINGS'
				if(s>=17)            //if cursor at or beyond 'Menu Frame'
					y+=FONT_HEIGHT/2;  //adjust for half-row space below 'Menu Title'
				if(s>=max_s-1)            //if cursor at or beyond 'RETURN'
					y+=FONT_HEIGHT/2;  //adjust for half-row space below 'Menu Frame'
			}
			drawChar(127, x, y/2, setting->color[3]);  //draw cursor

			//Tooltip section
			if (s <= 13) {  //if cursor at a colour component or a screen offset
				if (swapKeys)
					strcpy(c, "Å~:Add Åõ:Subtract");
				else
					strcpy(c, "Åõ:Add Å~:Subtract");
			} else if(s==14||s==17) {  //if cursor at 'INTERLACE' or 'Main Menu Frame'
				if (swapKeys)
					strcpy(c, "Å~:Change");
				else
					strcpy(c, "Åõ:Change");
			} else if(s==15){  //if cursor at 'SKIN SETTINGS'
				if (swapKeys)
					sprintf(c, "Å~:OK");
				else
					sprintf(c, "Åõ:OK");
			} else if(s==16){  //if cursor at Menu_Title
				if (swapKeys)
					sprintf(c, "Å~:Edit Åõ:Clear");
				else
					sprintf(c, "Åõ:Edit Å~:Clear");
			} else {  //if cursor at 'RETURN' or 'DEFAULT' options
				if (swapKeys)
					strcpy(c, "Å~:OK");
				else
					strcpy(c, "Åõ:OK");
			}
		
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
	int s, max_s=6;		//define cursor index and its max value
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
				if(s==2)
				{
					if(setting->numCNF > 1)
						setting->numCNF--;
				}
				else if(s==4) setting->usbd[0] = '\0';
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
					getFilePath(setting->usbd, USBD_IRX_CNF);
				else if(s==5)
					setting->TV_mode = (setting->TV_mode+1)%3; //Change between 0,1,2
				else
					return;
			}
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
				sprintf(c, "  RESET IOP: ON");
			else
				sprintf(c, "  RESET IOP: OFF");
			printXY(c, x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;

			sprintf(c, "  NUMBER OF CNF'S: %d", setting->numCNF);
			printXY(c, x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;

			if(setting->swapKeys)
				sprintf(c, "  KEY MAPPING: Å~:OK Åõ:CANCEL");
			else
				sprintf(c, "  KEY MAPPING: Åõ:OK Å~:CANCEL");
			printXY(c, x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;

			if(strlen(setting->usbd)==0)
				sprintf(c, "  USBD IRX: DEFAULT");
			else
				sprintf(c, "  USBD IRX: %s",setting->usbd);
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

			y += FONT_HEIGHT / 2;
			printXY("  RETURN", x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;

			//Cursor positioning section
			y = Menu_start_y + s*FONT_HEIGHT + FONT_HEIGHT /2;

			if(s>=max_s) y+=FONT_HEIGHT/2;
			drawChar(127, x, y/2, setting->color[3]);

			//Tooltip section
			if (s == 1) {
				if (swapKeys)
					strcpy(c, "Å~:Change");
				else
					strcpy(c, "Åõ:Change");
			} else if (s == 2) {
				if (swapKeys)
					strcpy(c, "Å~:Add Åõ:Subtract");
				else
					strcpy(c, "Åõ:Add Å~:Subtract");
			} else if(s == 3) {
				if (swapKeys)
					sprintf(c, "Å~:Change");
				else
					sprintf(c, "Åõ:Change");
			} else if(s == 4) {
				if (swapKeys)
					sprintf(c, "Å~:Select Åõ:Clear");
				else
					sprintf(c, "Åõ:Select Å~:Clear");
			} else if(s == 5) {
				if (swapKeys)
					sprintf(c, "Å~:Change");
				else
					sprintf(c, "Åõ:Change");
			} else {
				if (swapKeys)
					strcpy(c, "Å~:OK");
				else
					strcpy(c, "Åõ:OK");
			}
		
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
		}

		if(event||post_event){ //NB: We need to update two frame buffers per event

			//Display section
			clrScr(setting->color[0]);

			x = Menu_start_x;
			y = Menu_start_y;

			printXY("NETWORK SETTINGS", x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;

			y += FONT_HEIGHT / 2;

			sprintf(c, "  IP ADDRESS:  %.3i . %.3i . %.3i . %.3i",ipdata.ip[0],ipdata.ip[1],ipdata.ip[2],ipdata.ip[3]);
			printXY(c, x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;

			sprintf(c, "  NETMASK:     %.3i . %.3i . %.3i . %.3i",ipdata.nm[0],ipdata.nm[1],ipdata.nm[2],ipdata.nm[3]);
			printXY(c, x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;

			sprintf(c, "  GATEWAY:     %.3i . %.3i . %.3i . %.3i",ipdata.gw[0],ipdata.gw[1],ipdata.gw[2],ipdata.gw[3]);
			printXY(c, x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;

			y += FONT_HEIGHT / 2;

			sprintf(c, "  SAVE");
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
					strcpy(c, "Å~:Add Åõ:Subtract");
				else
					strcpy(c, "Åõ:Add Å~:Subtract");
			} else if( s== 4) {
				if (swapKeys)
					sprintf(c, "Å~:Save");
				else
					sprintf(c, "Åõ:Save");
			} else {
				if (swapKeys)
					strcpy(c, "Å~:OK");
				else
					strcpy(c, "Åõ:OK");
			}
	
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
					s=TIMEOUT;
				else
					s=DEFAULT;
			}
			else if(new_pad & PAD_RIGHT)
			{
				event |= 2;  //event |= valid pad command
				if(s<TIMEOUT)
					s=TIMEOUT;
				else if(s<OK)
					s=OK;
			}
			else if((!swapKeys && new_pad & PAD_CROSS) || (swapKeys && new_pad & PAD_CIRCLE) )
			{
				event |= 2;  //event |= valid pad command
				if(s<TIMEOUT)
					setting->dirElf[s][0]=0;
				else if(s==TIMEOUT)
				{
					if(setting->timeout > 0)
						setting->timeout--;
				}
			}
			else if((swapKeys && new_pad & PAD_CROSS) || (!swapKeys && new_pad & PAD_CIRCLE))
			{
				event |= 2;  //event |= valid pad command
				if(s<TIMEOUT)
				{
					getFilePath(setting->dirElf[s], TRUE);
					if(!strncmp(setting->dirElf[s], "mc0", 3) ||
						!strncmp(setting->dirElf[s], "mc1", 3)){
						sprintf(c, "mc%s", &setting->dirElf[s][3]);
						strcpy(setting->dirElf[s], c);
					}
				}
				else if(s==TIMEOUT)
					setting->timeout++;
				else if(s==FILENAME)
					setting->filename = !setting->filename;
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
				{
					free(setting);
					setting = tmpsetting;
					updateScreenMode();
					strcpy(setting->skin, skinSave);
					loadSkin(BACKGROUND_PIC);
					mainMsg[0] = 0;
					break;
				}
			}
		}
		
		if(event||post_event){ //NB: We need to update two frame buffers per event

			//Display section
			clrScr(setting->color[0]);

			x = Menu_start_x;
			y = Menu_start_y;
			printXY("BUTTON SETTING", x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;
			for(i=0; i<12; i++)
			{
				switch(i)
				{
				case 0:
					strcpy(c,"  DEFAULT: ");
					break;
				case 1:
					strcpy(c,"  Åõ     : ");
					break;
				case 2:
					strcpy(c,"  Å~     : ");
					break;
				case 3:
					strcpy(c,"  Å†     : ");
					break;
				case 4:
					strcpy(c,"  Å¢     : ");
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
				strcat(c, setting->dirElf[i]);
				printXY(c, x, y/2, setting->color[3], TRUE);
				y += FONT_HEIGHT;
			}

			y += FONT_HEIGHT / 2;

			sprintf(c, "  TIMEOUT: %d", setting->timeout);
			printXY(c, x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;

			if(setting->discControl)
				sprintf(c, "  DISC CONTROL: ON");
			else
				sprintf(c, "  DISC CONTROL: OFF");
			printXY(c, x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;

			if(setting->filename)
				sprintf(c, "  PRINT ONLY FILENAME: ON");
			else
				sprintf(c, "  PRINT ONLY FILENAME: OFF");
			printXY(c, x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;

			printXY("  SCREEN SETTINGS", x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;
			printXY("  STARTUP SETTINGS", x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;
			printXY("  NETWORK SETTINGS", x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;

			printXY("  OK", x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;
			printXY("  CANCEL", x, y/2, setting->color[3], TRUE);

			//Cursor positioning section
			y = Menu_start_y + (s+1)*FONT_HEIGHT;
			if(s>=TIMEOUT)
				y += FONT_HEIGHT / 2;
			drawChar(127, x, y/2, setting->color[3]);

			//Tooltip section
			if (s < TIMEOUT) {
				if (swapKeys)
					sprintf(c, "Å~:Edit Åõ:Clear");
				else
					sprintf(c, "Åõ:Edit Å~:Clear");
			} else if(s==TIMEOUT) {
				if (swapKeys)
					sprintf(c, "Å~:Add Åõ:Subtract");
				else
					sprintf(c, "Åõ:Add Å~:Subtract");
			} else if(s==FILENAME) { 
				if (swapKeys)
					sprintf(c, "Å~:Change");
				else
					sprintf(c, "Åõ:Change");
			} else if(s==DISCCONTROL) {
				if (swapKeys)
					sprintf(c, "Å~:Change");
				else
					sprintf(c, "Åõ:Change");
			} else {
				if (swapKeys)
					sprintf(c, "Å~:OK");
				else
					sprintf(c, "Åõ:OK");
			}

			setScrTmp("", c);
		}//ends if(event||post_event)
		drawScr();
		post_event = event;
		event = 0;

	}//ends while
}//ends config
//---------------------------------------------------------------------------
// End of file: config.c
//---------------------------------------------------------------------------
