//--------------------------------------------------------------
//File name:    editor.c
//--------------------------------------------------------------
#include "launchelf.h"

enum
{
	COL_NORM_BG    = ITO_RGBA(160,160,160,0),
	COL_NORM_TEXT  = ITO_RGBA(0,0,0,0),
	COL_MARK_BG    = ITO_RGBA(0,40,160,0),
	COL_MARK_TEXT  = ITO_RGBA(160,160,160,0),
	COL_CUR_INSERT = ITO_RGBA(0,160,0,0),
	COL_CUR_OVERWR = ITO_RGBA(160,0,0,0),
	COL_LINE_END   = ITO_RGBA(160,80,0,0),
	COL_TAB        = ITO_RGBA(0,0,160,0),
	COL_TEXT_END   = ITO_RGBA(0,160,160,0),
	COL_dummy
};

enum
{
	NEW,
	OPEN,
	CLOSE,
	SAVE,
	SAVE_AS,
	WINDOWS,
	EXIT,
	NUM_MENU
}; // Menu Fonction.

enum
{
	MARK_START,
	MARK_ON,
	MARK_COPY,
	MARK_CUT,
	MARK_IN,
	MARK_OUT,
	MARK_TMP,
	MARK_SIZE,
	MARK_PRINT,
	MARK_COLOR,
	NUM_MARK
}; // Marking Fonction/State.

enum
{
	CREATED,
	OPENED,
	SAVED,
	NUM_STATE
}; // Windowing State.

#define OTHER 1 // CR/LF Return Text Mode, 'All Normal System'.
#define UNIX  2 // CR Return Text Mode, Unix.
#define MAC   3 // LF Return Text Mode, Mac.

#define TMP  10 // Temp Buffer For Add / Remove Char.
#define EDIT 11 // Edit Buffer For Copy / Cut / Paste.

static u8   *TextBuffer[12]; // Text Buffers, 10 Windows Max + 1 TMP + 1 EDIT. See above.
static int  Window[10][NUM_STATE], // Windowing System, 10 Windows Max.
						TextMode[10], // Text Mode, UNIX, MAC, OTHER.
						TextSize[10], // Text Size, 10 Windows Max.
						Editor_nRowsWidth[MAX_ENTRY], // Current Window, Char Number For Each Rows. (MAX_ENTRY???)
						Mark[NUM_MARK], // Marking System,(Mark, Copy, Cut, Paste, Delete),Work From 1 Window To An Other.
						Active_Window, // Activated Windows Number.
						Num_Window, // Opened Windows Count.
						Editor_Cur, // Text Cursor.
						Tmp_Cur, // Temp Cursor For Rules Calculations.
						Editor_nChar, // Current Window, Total Char Number.
						Editor_nRowsNum, // Current Window, Total Rows Number.
						Editor_nCharRows, // Current Window, Char Number Per Rows Height.
						Editor_nRowsTop, // Current Window, Rows Number Above Top Screen For Rules Calculations.
						Rows_Num, // Rows Number Feeting In Screen Height.
						Rows_Width, // Char Number Feeting In Screen Width.
						Top_Width,  // Char Number In Rows Above Top Screen.
						Top_Height, // Current Window Rows Number Above Top Screen.
						Editor_Insert, // Setting Insert On/Off.
						Editor_RetMode, // Setting Insert On/Off.
						Editor_Home, // Goto Line Home.
						Editor_End, // Goto Line End
						Editor_PushRows, // Push 1 Row Height Up(+1) Or Down(-1), Use For Page Up/Down Too.
						Editor_TextEnd, // Set To 1 When '\0' Char Is Found Tell Text End.
						KeyBoard_Cur, // Virtual KeyBoard Cursor.
						KeyBoard_Active, // Virtual KeyBoard Activated Or Not.
						del1, del2, del3, del4, // Deleted Chars Different Cases.
						ins1, ins2, ins3, ins4, ins5, // Added Chars Different Cases.
						t; // Text Cursor Timer.
static char Path[10][MAX_PATH]; // File Path For Each Opened Windows. 10 Max.

static const int WFONTS=20, // Virtual KeyBoard Width.
								 HFONTS=5; // Virtual KeyBoard Height.
static char *KEY= "  01ABCDEFGHIJKLM:; "
									"  23NOPQRSTUVWXYZ., "
									"  45abcdefghijklm() "
									"  67nopqrstuvwxyz[] "
									"  89+-=!#   $%&@_^' "; // Virtual KeyBoard Matrix.

//--------------------------------------------------------------
int MenuEditor(void)
{
	uint64 color;
	char enable[NUM_MENU], tmp[64];
	int x, y, i, Menu_Sel;
	int event, post_event=0;

	int menu_ch_w = 7;             //Total characters in longest menu string.
	int menu_ch_h = NUM_MENU;      //Total number of menu lines.
	int mSprite_Y1 = 32;           //Top edge of sprite.
	int mSprite_X2 = SCREEN_WIDTH-35;   //Right edge of sprite.
	int mFrame_Y1 = mSprite_Y1;  //Top edge of frame.
	int mFrame_X2 = mSprite_X2-3;  //Right edge of frame (-3 correct ???).
	int mFrame_X1 = mFrame_X2-(menu_ch_w+3)*FONT_WIDTH;    //Left edge of frame.
	int mFrame_Y2 = mFrame_Y1+(menu_ch_h+1)*FONT_HEIGHT/2; //Bottom edge of frame.
	int mSprite_X1 = mFrame_X1-1;  //Left edge of sprite.
	int mSprite_Y2 = mFrame_Y2;  //Bottom edge of sprite.

	memset(enable, TRUE, NUM_MENU);

	if(!Window[Active_Window][OPENED]){
		enable[CLOSE] = FALSE;
		enable[SAVE] = FALSE;
		enable[SAVE_AS] = FALSE;
	}	
	if(Window[Active_Window][CREATED]){
		enable[SAVE] = FALSE;
	}
	if(!Num_Window)
		enable[WINDOWS] = FALSE;

	for(Menu_Sel=0; Menu_Sel<NUM_MENU; Menu_Sel++)
		if(enable[Menu_Sel]==TRUE) break;
	
	event = 1;  //event = initial entry.
	while(1){
		//Pad response section.
		waitPadReady(0, 0);
		if(readpad()){
			if(new_pad & PAD_UP && Menu_Sel<NUM_MENU){
				event |= 2;  //event |= valid pad command.
				do{
					Menu_Sel--;
					if(Menu_Sel<0) Menu_Sel=NUM_MENU-1;
				}while(!enable[Menu_Sel]);
			}else if(new_pad & PAD_DOWN && Menu_Sel<NUM_MENU){
				event |= 2;  //event |= valid pad command.
				do{
					Menu_Sel++;
					if(Menu_Sel==NUM_MENU) Menu_Sel=0;
				}while(!enable[Menu_Sel]);
			}else if((new_pad & PAD_TRIANGLE)
						|| (!swapKeys && new_pad & PAD_CROSS)
			      || (swapKeys && new_pad & PAD_CIRCLE) ){
				return -1;
			}else if((swapKeys && new_pad & PAD_CROSS)
			      || (!swapKeys && new_pad & PAD_CIRCLE) ){
				event |= 2;  //event |= valid pad command.
				break;
			}
		}

		if(event||post_event){ //NB: We need to update two frame buffers per event.

			//Display section.
			drawPopSprite(setting->color[0],
				mSprite_X1, mSprite_Y1,
				mSprite_X2, mSprite_Y2);
			drawFrame(mFrame_X1, mFrame_Y1, mFrame_X2, mFrame_Y2, setting->color[1]);

			for(i=0,y=mFrame_Y1*2+FONT_HEIGHT/2; i<NUM_MENU; i++){
				if(i==NEW)			strcpy(tmp, "New");
				else if(i==OPEN)			strcpy(tmp, "Open");
				else if(i==CLOSE)		strcpy(tmp, "Close");
				else if(i==SAVE)		strcpy(tmp, "Save");
				else if(i==SAVE_AS)	strcpy(tmp, "Save_As");
				else if(i==WINDOWS)	strcpy(tmp, "Windows");
				else if(i==EXIT)	strcpy(tmp, "Exit");

				if(enable[i])	color = setting->color[3];
				else			color = setting->color[1];

				printXY(tmp, mFrame_X1+2*FONT_WIDTH, y/2, color, TRUE);
				y+=FONT_HEIGHT;
			}
			if(Menu_Sel<NUM_MENU)
				drawChar(127, mFrame_X1+FONT_WIDTH, mFrame_Y1+(FONT_HEIGHT/2+Menu_Sel*FONT_HEIGHT)/2, setting->color[3]);

			//Tooltip section.
			x = SCREEN_MARGIN;
			y = Menu_tooltip_y;
			drawSprite(setting->color[0],
				0, (y/2),
				SCREEN_WIDTH, y/2+10);
			if (swapKeys)
				printXY("ÿ1:OK ÿ0:Cancel ÿ3:Back", x, y/2, setting->color[2], TRUE);
			else
				printXY("ÿ0:OK ÿ1:Cancel ÿ3:Back", x, y/2, setting->color[2], TRUE);
		}//ends if(event||post_event).
		drawScr();
		post_event = event;
		event = 0;
	}//ends while.
	return Menu_Sel;
}//ends menu.
//--------------------------------------------------------------
void Virt_KeyBoard_Entry(void)
{
	int i, Operation=0;

	if(new_pad & PAD_UP){ // Virtual KeyBoard move up.
		if(!KeyBoard_Cur)
			KeyBoard_Cur=WFONTS*(HFONTS-1);
		else if(KeyBoard_Cur==WFONTS-1)
			KeyBoard_Cur=WFONTS*HFONTS-1;
		else if(KeyBoard_Cur>0){
			if((KeyBoard_Cur-=WFONTS)<0)
				KeyBoard_Cur=0;
		}
		//ends Virtual KeyBoard move up.
	}else if(new_pad & PAD_DOWN){ // Virtual KeyBoard move down.
		if(KeyBoard_Cur==WFONTS*HFONTS-1)
			KeyBoard_Cur=WFONTS-1;
		else if(KeyBoard_Cur==WFONTS*(HFONTS-1))
			KeyBoard_Cur=0;
		else if(KeyBoard_Cur<WFONTS*HFONTS-1){
			if((KeyBoard_Cur+=WFONTS)>WFONTS*HFONTS-1)
				KeyBoard_Cur=WFONTS*HFONTS-1;
		}
		//ends Virtual KeyBoard move down.
	}else if(new_pad & PAD_LEFT){ // Virtual KeyBoard move left.
		if(!KeyBoard_Cur)
			KeyBoard_Cur=WFONTS*HFONTS-1;
		else
			KeyBoard_Cur--;
		//ends Virtual KeyBoard move left.
	}else if(new_pad & PAD_RIGHT){ // Virtual KeyBoard move right.
		if(KeyBoard_Cur==WFONTS*HFONTS-1)
			KeyBoard_Cur=0;
		else
			KeyBoard_Cur++;
		//ends Virtual KeyBoard move right.
	}else if(new_pad & PAD_L2){ // Text move left.
		if(Editor_Cur>0)
			Editor_Cur--;
	}else if(new_pad & PAD_R2){ // Text move right.
		if(Editor_Cur<Editor_nChar && TextBuffer[Active_Window][Editor_Cur]!='\0')
			Editor_Cur++;
	}else if(new_pad & PAD_SELECT){ // Virtual KeyBoard Exit.
		Rows_Num += 6;
		KeyBoard_Active = 0;
	}else if((!swapKeys && new_pad & PAD_CROSS)
	      || (swapKeys && new_pad & PAD_CIRCLE) ){ // Virtual KeyBoard Suppr.
		if(Editor_Cur>0){
			if(Mark[MARK_ON]){
				Mark[MARK_OUT]=Editor_Cur;
				if(TextMode[Active_Window]==OTHER && TextBuffer[Active_Window][Mark[MARK_OUT]]=='\n')
					Mark[MARK_OUT]=Editor_Cur+1;
				if(Mark[MARK_OUT]<Mark[MARK_IN]){
					Mark[MARK_TMP]=Mark[MARK_IN];
					Mark[MARK_IN]=Mark[MARK_OUT];
					Mark[MARK_OUT]=Mark[MARK_TMP];
				}else if(Mark[MARK_IN]==Mark[MARK_OUT])
					goto abort;
				Mark[MARK_SIZE]=Mark[MARK_OUT]-Mark[MARK_IN];
				del1= -Mark[MARK_SIZE], del2=0, del3= -Mark[MARK_SIZE], del4= -Mark[MARK_SIZE];
				Mark[MARK_ON]=0;
			}else if(TextMode[Active_Window]==OTHER && TextBuffer[Active_Window][Editor_Cur]=='\n'){
				del1=-1, del2=1, del3=-2, del4=-1;
			}else if(TextMode[Active_Window]==OTHER && TextBuffer[Active_Window][Editor_Cur-1]=='\n'){
				del1=-2, del2=0, del3=-2, del4=-2;
			}else{
				del1=-1, del2=0, del3=-1, del4=-1;
			}
			Operation=-1;
		}
		//ends Virtual KeyBoard Suppr.
	}else if((swapKeys && new_pad & PAD_CROSS)
	      || (!swapKeys && new_pad & PAD_CIRCLE) ){ // Virtual KeyBoard Select.
		if(!KeyBoard_Cur){ // Virtual KeyBoard MARK.
			Mark[MARK_ON]=!Mark[MARK_ON];
			if(Mark[MARK_ON]){
				if(Mark[MARK_COPY] || Mark[MARK_CUT])
					free(TextBuffer[EDIT]);
				Mark[MARK_ON]=1, Mark[MARK_COPY]=0, Mark[MARK_CUT]=0,
				Mark[MARK_IN]=0, Mark[MARK_OUT]=0, Mark[MARK_TMP]=0,
				Mark[MARK_SIZE]=0, Mark[MARK_PRINT]=0, Mark[MARK_COLOR]=0;
				Mark[MARK_IN]=Mark[MARK_OUT]=Editor_Cur;
				if(TextMode[Active_Window]==OTHER && TextBuffer[Active_Window][Mark[MARK_IN]]=='\n')
					Mark[MARK_IN]=Mark[MARK_OUT]=Editor_Cur-1;
			}
			Mark[MARK_START]=1;
			//ends Virtual KeyBoard MARK.
		}else if(KeyBoard_Cur == WFONTS){ // Virtual KeyBoard COPY.
			if(Mark[MARK_ON]){
				Mark[MARK_OUT]=Editor_Cur;
				if(TextMode[Active_Window]==OTHER && TextBuffer[Active_Window][Mark[MARK_OUT]]=='\n')
					Mark[MARK_OUT]=Editor_Cur+1;
				if(Mark[MARK_OUT]<Mark[MARK_IN]){
					Mark[MARK_TMP]=Mark[MARK_IN];
					Mark[MARK_IN]=Mark[MARK_OUT];
					Mark[MARK_OUT]=Mark[MARK_TMP];
				}else if(Mark[MARK_IN]==Mark[MARK_OUT])
					goto abort;
				if(Mark[MARK_COPY] || Mark[MARK_CUT])
					free(TextBuffer[EDIT]);
				Mark[MARK_SIZE]=Mark[MARK_OUT]-Mark[MARK_IN];
				TextBuffer[EDIT] = malloc(Mark[MARK_SIZE]+256); // 256 To Avoid Crash 256???
				for(i=0; i<Mark[MARK_SIZE]; i++)
					TextBuffer[EDIT][i]=TextBuffer[Active_Window][i+Mark[MARK_IN]];
				Mark[MARK_COPY]=1, Mark[MARK_CUT]=0, Mark[MARK_ON]=0;
			}
			//ends Virtual KeyBoard COPY.
		}else if(KeyBoard_Cur == 2*WFONTS){ // Virtual KeyBoard CUT.
			if(Mark[MARK_ON]){
				Mark[MARK_OUT]=Editor_Cur;
				if(TextMode[Active_Window]==OTHER && TextBuffer[Active_Window][Mark[MARK_OUT]]=='\n')
					Mark[MARK_OUT]=Editor_Cur+1;
				if(Mark[MARK_OUT]<Mark[MARK_IN]){
					Mark[MARK_TMP]=Mark[MARK_IN];
					Mark[MARK_IN]=Mark[MARK_OUT];
					Mark[MARK_OUT]=Mark[MARK_TMP];
				}else if(Mark[MARK_IN]==Mark[MARK_OUT])
					goto abort;
				if(Mark[MARK_COPY] || Mark[MARK_CUT])
					free(TextBuffer[EDIT]);
				Mark[MARK_SIZE]=Mark[MARK_OUT]-Mark[MARK_IN];
				TextBuffer[EDIT] = malloc(Mark[MARK_SIZE]+256); // 256 To Avoid Crash 256???
				for(i=0; i<Mark[MARK_SIZE]; i++)
					TextBuffer[EDIT][i]=TextBuffer[Active_Window][i+Mark[MARK_IN]];
				del1= -Mark[MARK_SIZE], del2=0, del3= -Mark[MARK_SIZE], del4= -Mark[MARK_SIZE];
				Mark[MARK_CUT]=1, Mark[MARK_COPY]=0, Mark[MARK_ON]=0;
				Operation=-1;
			}
abort:
			Mark[MARK_TMP]=0; // just for compiler warning.
			//ends Virtual KeyBoard CUT.
		}else if(KeyBoard_Cur == 3*WFONTS){ // Virtual KeyBoard PASTE.
			if( Mark[MARK_COPY] || Mark[MARK_CUT]){
				if(TextMode[Active_Window]==OTHER && TextBuffer[Active_Window][Editor_Cur]=='\n'){
					Editor_Cur-=1;
					Mark[MARK_SIZE]-=1;
				}
				ins1=Mark[MARK_SIZE], ins2=Mark[MARK_SIZE], ins3=Mark[MARK_SIZE], ins4=0, ins5=Mark[MARK_SIZE];
				Operation=1;
			}
			//ends Virtual KeyBoard PASTE.
		}else if(KeyBoard_Cur == 4*WFONTS){ // Virtual KeyBoard HOME.
			Editor_Home=1;
		}else if(KeyBoard_Cur == 1){ // Virtual KeyBoard LINE UP.
			if(Editor_Cur>0)
				Editor_PushRows++;
		}else if(KeyBoard_Cur == WFONTS+1){ // Virtual KeyBoard LINE DOWN.
			if(Editor_Cur<Editor_nChar)
				Editor_PushRows--;
		}else if(KeyBoard_Cur == 2*WFONTS+1){ // Virtual KeyBoard PAGE UP.
			if(Editor_Cur>0)
				Editor_PushRows += 2*(Rows_Num-1);
		}else if(KeyBoard_Cur == 3*WFONTS+1){ // Virtual KeyBoard PAGE DOWN.
			if(Editor_Cur<Editor_nChar && TextBuffer[Active_Window][Editor_Cur]!='\0')
				Editor_PushRows -= 2*(Rows_Num-1);
		}else if(KeyBoard_Cur == 4*WFONTS+1){ // Virtual KeyBoard END.
			Editor_End=1;
		}else if(KeyBoard_Cur == WFONTS-1){ // Virtual KeyBoard INSERT.
			Editor_Insert = !Editor_Insert;
		}else if(KeyBoard_Cur == 2*WFONTS-1){ // Virtual KeyBoard Return Mode CR/LF, LF, CR.
			if((Editor_RetMode++)>=4)
				Editor_RetMode=OTHER;
		}else if(KeyBoard_Cur == 5*WFONTS-1){ // Virtual KeyBoard RETURN.
			if(TextMode[Active_Window]==OTHER && TextBuffer[Active_Window][Editor_Cur]=='\n' &&
			 Editor_Insert && Editor_RetMode==OTHER){
				ins1= 2, ins2=-1, ins3=1, ins4=-1, ins5=1;
			}else if(TextMode[Active_Window]==OTHER && TextBuffer[Active_Window][Editor_Cur]=='\n' &&
			 Editor_Insert){
				ins1= 1, ins2=-2, ins3=2, ins4=-1, ins5=0;
			}else if(TextMode[Active_Window]==OTHER && TextBuffer[Active_Window][Editor_Cur]=='\n' &&
			 Editor_RetMode==OTHER){
				ins1= 0, ins2=-1, ins3= 1, ins4=1, ins5=1;
			}else if(TextMode[Active_Window]==OTHER && TextBuffer[Active_Window][Editor_Cur]=='\n'){
				ins1= 0, ins2=-2, ins3=0, ins4=1, ins5=0;
			}else if(TextMode[Active_Window]==OTHER && TextBuffer[Active_Window][Editor_Cur+1]=='\n' &&
			 Editor_RetMode==OTHER){
				ins1= 1, ins2= 0, ins3= 2, ins4=2, ins5=2;
			}else if(TextMode[Active_Window]==OTHER && TextBuffer[Active_Window][Editor_Cur+1]=='\n'){
				ins1= 0, ins2=-1, ins3= 1, ins4=2, ins5=1;
			}else if((Editor_Insert || TextBuffer[Active_Window][Editor_Cur]=='\0') && Editor_RetMode==OTHER){
				ins1= 2, ins2= 0, ins3= 2, ins4=0, ins5=2;
			}else if(Editor_Insert || TextBuffer[Active_Window][Editor_Cur]=='\0'){
				ins1= 1, ins2=-1, ins3= 1, ins4=0, ins5=1;
			}else if(Editor_RetMode==OTHER){
				ins1= 1, ins2= 0, ins3= 2, ins4=1, ins5=2;
			}else{
				ins1= 0, ins2=-1, ins3= 1, ins4=1, ins5=1;
			}
			Operation=2;
			//ends Virtual KeyBoard RETURN.
		}else{  // Virtual KeyBoard Any other char + Space + Tabulation.
			if(TextMode[Active_Window]==OTHER && TextBuffer[Active_Window][Editor_Cur]=='\n'){
				ins1=0, ins2=-1, ins3=0, ins4= 1, ins5= 0;
			}else if(TextMode[Active_Window]==OTHER && TextBuffer[Active_Window][Editor_Cur+1]=='\n' && !Editor_Insert){
				ins1=0, ins2= 0, ins3=1, ins4= 2, ins5= 1;
			}else if(TextMode[Active_Window]==OTHER && TextBuffer[Active_Window][Editor_Cur]=='\n' && Editor_Insert){
				ins1=1, ins2=-1, ins3=0, ins4=-1, ins5=-1;
			}else if(Editor_Insert || TextBuffer[Active_Window][Editor_Cur]=='\0'){
				ins1=1, ins2= 0, ins3=1, ins4= 0, ins5= 1;
			}else{
				ins1=0, ins2= 0, ins3=1, ins4= 1, ins5= 1;
			}
			if(KeyBoard_Cur == 3*WFONTS-1) // Tabulation.
				Operation=3;
			else if(KeyBoard_Cur == 4*WFONTS-1) // Space.
				Operation=4;
			else // Any other char.
				Operation=5;
		}
		//ends Virtual KeyBoard Select.
	}
	if(Operation>0){ // Perform Add Char / Paste. Can Be Simplify???
		TextBuffer[TMP] = malloc(TextSize[Active_Window]+ins1+256); // 256 To Avoid Crash 256???
		strcpy(TextBuffer[TMP], TextBuffer[Active_Window]);
		memset(TextBuffer[Active_Window], 0, TextSize[Active_Window]+256); // 256 To Avoid Crash 256???
		free(TextBuffer[Active_Window]);
		TextBuffer[Active_Window] = malloc(TextSize[Active_Window]+ins1+256); // 256 To Avoid Crash 256???
		strcpy(TextBuffer[Active_Window], TextBuffer[TMP]);
		if(Operation==1){ // Paste.
			for(i=0; i<ins2; i++)
				TextBuffer[Active_Window][i+Editor_Cur]=TextBuffer[EDIT][i];
		}else if(Operation==2){ // Return.
			if(Editor_RetMode==OTHER){
				TextBuffer[Active_Window][Editor_Cur+ins2]='\r';
				TextBuffer[Active_Window][Editor_Cur+ins2+1]='\n';
			}else if(Editor_RetMode==UNIX){
				TextBuffer[Active_Window][Editor_Cur+ins2+1]='\r';
			}else if(Editor_RetMode==MAC){
				TextBuffer[Active_Window][Editor_Cur+ins2+1]='\n';
			}
		}else if(Operation==3) // Tabulation.
			TextBuffer[Active_Window][Editor_Cur+ins2]='\t';
		else if(Operation==4) // Space.
			TextBuffer[Active_Window][Editor_Cur+ins2]=' ';
		else if(Operation==5) // Any Char.
			TextBuffer[Active_Window][Editor_Cur+ins2]=KEY[KeyBoard_Cur];
		TextBuffer[Active_Window][Editor_Cur+ins3]='\0';
		strcat(TextBuffer[Active_Window], TextBuffer[TMP]+(Editor_Cur+ins4));
		memset(TextBuffer[TMP], 0, TextSize[Active_Window]+256); // 256 To Avoid Crash 256???
		free(TextBuffer[TMP]);
		Editor_Cur+=ins5, TextSize[Active_Window]+=ins1;
		t=0;
	}else if(Operation<0){ // Perform Del Char / Cut. Can Be Simplify???
		TextBuffer[TMP] = malloc(TextSize[Active_Window]+256); // 256 To Avoid Crash 256???
		strcpy(TextBuffer[TMP], TextBuffer[Active_Window]);
		TextBuffer[Active_Window][Editor_Cur+del1]='\0';
		strcat(TextBuffer[Active_Window], TextBuffer[TMP]+(Editor_Cur+del2));
		strcpy(TextBuffer[TMP], TextBuffer[Active_Window]);
		memset(TextBuffer[Active_Window], 0, TextSize[Active_Window]+256); // 256 To Avoid Crash 256???
		free(TextBuffer[Active_Window]);
		TextBuffer[Active_Window] = malloc(TextSize[Active_Window]+del3+256); // 256 To Avoid Crash 256???
		strcpy(TextBuffer[Active_Window], TextBuffer[TMP]);
		memset(TextBuffer[TMP], 0, TextSize[Active_Window]+256); // 256 To Avoid Crash 256???
		free(TextBuffer[TMP]);
		Editor_Cur+=del3, TextSize[Active_Window]+=del4;
		t=0;
	}
	Operation=0;
}
//------------------------------
//endfunc Virt_KeyBoard_Entry
//--------------------------------------------------------------
int KeyBoard_Entry(void)
{
	int i, ret=0, Operation=0;
	unsigned char KeyPress;

	if(PS2KbdRead(&KeyPress)) { //KeyBoard Response Section.

		ret=1;  // Equal To event |= pad command.

		if(KeyPress == PS2KBD_ESCAPE_KEY) {
			PS2KbdRead(&KeyPress);
			if(KeyPress)
				t=0;
			if(KeyPress == 0x29){ // Key Right.
				if(Editor_Cur<Editor_nChar && TextBuffer[Active_Window][Editor_Cur]!='\0')
					Editor_Cur++;
			}else if(KeyPress == 0x2A){ // Key Left.
				if(Editor_Cur>0)
					Editor_Cur--;
			}else if(KeyPress == 0x2C){ // Key Up.
				if(Editor_Cur>0)
					Editor_PushRows++;
			}else if(KeyPress == 0x2B){ // Key Down.
				if(Editor_Cur<Editor_nChar && TextBuffer[Active_Window][Editor_Cur]!='\0')
					Editor_PushRows--;
			}else if(KeyPress == 0x24) // Key Home.
				Editor_Home=1;
			else if(KeyPress == 0x27) // Key End.
				Editor_End=1;
			else if(KeyPress == 0x25){ // Key PgUp.
				if(Editor_Cur>0)
					Editor_PushRows += 2*(Rows_Num-1);
			}else if(KeyPress == 0x28){ // Key PgDn.
				if(Editor_Cur<Editor_nChar && TextBuffer[Active_Window][Editor_Cur]!='\0')
					Editor_PushRows -= 2*(Rows_Num-1);
			}else if(KeyPress == 0x23) // Key Insert.
				Editor_Insert = !Editor_Insert;
			else if(KeyPress == 0x26){ // Key Delete.
				if(Editor_Cur<Editor_nChar){
					if(Mark[MARK_ON]){
						Mark[MARK_OUT]=Editor_Cur;
						if(Mark[MARK_OUT]<Mark[MARK_IN]){
							Mark[MARK_TMP]=Mark[MARK_IN];
							Mark[MARK_IN]=Mark[MARK_OUT];
							Mark[MARK_OUT]=Mark[MARK_TMP];
						}else if(Mark[MARK_IN]==Mark[MARK_OUT])
							goto abort;
						Mark[MARK_SIZE]=Mark[MARK_OUT]-Mark[MARK_IN];
						del1= -Mark[MARK_SIZE], del2=0, del3= -Mark[MARK_SIZE], del4= -Mark[MARK_SIZE];
						Mark[MARK_ON]=0;
					}else if(TextMode[Active_Window]==OTHER && TextBuffer[Active_Window][Editor_Cur+1]=='\n'){
						del1=0, del2=2, del3=0, del4=-2;
					}else if(TextMode[Active_Window]==OTHER && TextBuffer[Active_Window][Editor_Cur]=='\n'){
						del1=-1, del2=1, del3=-1, del4=-2;
					}else{
						del1=0, del2=1, del3=0, del4=-1;
					}
					Operation=-1;
				}
			}else if(KeyPress == 0x01){ // Key F1 MENU.
				ret=2;
			}else if(KeyPress == 0x1B){ // Key Escape EXIT Editor.
				ret=3;
			}
		}else{
			if(KeyPress == 0x12){ // Key Ctrl+r Return Mode CR/LF Or LF Or CR.
				if((Editor_RetMode++)>=4)
					Editor_RetMode=OTHER;
			}else if(KeyPress == 0x02){ // Key Ctrl+b MARK.
				Mark[MARK_ON]=!Mark[MARK_ON];
				if(Mark[MARK_ON]){
					if(Mark[MARK_COPY] || Mark[MARK_CUT])
						free(TextBuffer[EDIT]);
					Mark[MARK_ON]=1, Mark[MARK_COPY]=0, Mark[MARK_CUT]=0,
					Mark[MARK_IN]=0, Mark[MARK_OUT]=0, Mark[MARK_TMP]=0,
					Mark[MARK_SIZE]=0, Mark[MARK_PRINT]=0, Mark[MARK_COLOR]=0;
					Mark[MARK_IN]=Mark[MARK_OUT]=Editor_Cur;
					if(TextMode[Active_Window]==OTHER && TextBuffer[Active_Window][Mark[MARK_IN]]=='\n')
						Mark[MARK_IN]=Mark[MARK_OUT]=Editor_Cur-1;
				}
				Mark[MARK_START]=1;
				//ends Key Ctrl+b MARK.
			}else if(KeyPress == 0x03){ // Key Ctrl+c COPY.
				if(Mark[MARK_ON]){
					Mark[MARK_OUT]=Editor_Cur;
					if(TextMode[Active_Window]==OTHER && TextBuffer[Active_Window][Mark[MARK_OUT]]=='\n')
						Mark[MARK_OUT]=Editor_Cur+1;
					if(Mark[MARK_OUT]<Mark[MARK_IN]){
						Mark[MARK_TMP]=Mark[MARK_IN];
						Mark[MARK_IN]=Mark[MARK_OUT];
						Mark[MARK_OUT]=Mark[MARK_TMP];
					}else if(Mark[MARK_IN]==Mark[MARK_OUT])
						goto abort;
					if(Mark[MARK_COPY] || Mark[MARK_CUT])
						free(TextBuffer[EDIT]);
					Mark[MARK_SIZE]=Mark[MARK_OUT]-Mark[MARK_IN];
					TextBuffer[EDIT] = malloc(Mark[MARK_SIZE]+256); // 256 To Avoid Crash 256???
					for(i=0; i<Mark[MARK_SIZE]; i++)
						TextBuffer[EDIT][i]=TextBuffer[Active_Window][i+Mark[MARK_IN]];
					Mark[MARK_COPY]=1, Mark[MARK_CUT]=0, Mark[MARK_ON]=0, Mark[MARK_TMP]=0;
				}
				//ends Key Ctrl+c COPY.
			}else if(KeyPress == 0x18){ // Key Ctrl+x CUT.
				if(Mark[MARK_ON]){
					Mark[MARK_OUT]=Editor_Cur;
					if(TextMode[Active_Window]==OTHER && TextBuffer[Active_Window][Mark[MARK_OUT]]=='\n')
						Mark[MARK_OUT]=Editor_Cur+1;
					if(Mark[MARK_OUT]<Mark[MARK_IN]){
						Mark[MARK_TMP]=Mark[MARK_IN];
						Mark[MARK_IN]=Mark[MARK_OUT];
						Mark[MARK_OUT]=Mark[MARK_TMP];
					}else if(Mark[MARK_IN]==Mark[MARK_OUT])
						goto abort;
					if(Mark[MARK_COPY] || Mark[MARK_CUT])
						free(TextBuffer[EDIT]);
					Mark[MARK_SIZE]=Mark[MARK_OUT]-Mark[MARK_IN];
					TextBuffer[EDIT] = malloc(Mark[MARK_SIZE]+256); // 256 To Avoid Crash 256???
					for(i=0; i<Mark[MARK_SIZE]; i++)
						TextBuffer[EDIT][i]=TextBuffer[Active_Window][i+Mark[MARK_IN]];
					del1= -Mark[MARK_SIZE], del2=0, del3= -Mark[MARK_SIZE], del4= -Mark[MARK_SIZE];
					Mark[MARK_CUT]=1, Mark[MARK_COPY]=0, Mark[MARK_ON]=0, Mark[MARK_TMP]=0;
					Operation=-2;
				}
				//ends Key Ctrl+x CUT.
			}else if(KeyPress == 0x16){ // Key Ctrl+v PASTE.
				if( Mark[MARK_COPY] || Mark[MARK_CUT]){
					if(TextMode[Active_Window]==OTHER && TextBuffer[Active_Window][Editor_Cur]=='\n'){
						Editor_Cur-=1;
						Mark[MARK_SIZE]-=1;
					}
					ins1=Mark[MARK_SIZE], ins2=Mark[MARK_SIZE], ins3=Mark[MARK_SIZE], ins4=0, ins5=Mark[MARK_SIZE];
					Operation=1;
				}
				//ends Key Ctrl+v PASTE.
			}else if(KeyPress == 0x07){ // Key BackSpace.
				if(Editor_Cur>0){
					if(Mark[MARK_ON]){
						Mark[MARK_OUT]=Editor_Cur;
						if(Mark[MARK_OUT]<Mark[MARK_IN]){
							Mark[MARK_TMP]=Mark[MARK_IN];
							Mark[MARK_IN]=Mark[MARK_OUT];
							Mark[MARK_OUT]=Mark[MARK_TMP];
						}else if(Mark[MARK_IN]==Mark[MARK_OUT])
							goto abort;
						Mark[MARK_SIZE]=Mark[MARK_OUT]-Mark[MARK_IN];
						del1= -Mark[MARK_SIZE], del2=0, del3= -Mark[MARK_SIZE], del4= -Mark[MARK_SIZE];
						Mark[MARK_ON]=0;
					}else if(TextMode[Active_Window]==OTHER && TextBuffer[Active_Window][Editor_Cur]=='\n'){
						del1=-1, del2=1, del3=-1, del4=-2;
					}else if(TextMode[Active_Window]==OTHER && TextBuffer[Active_Window][Editor_Cur-1]=='\n'){
						del1=-2, del2=0, del3=-2, del4=-2;
					}else{
						del1=-1, del2=0, del3=-1, del4=-1;
					}
					Operation=-3;
				}
				//ends Key BackSpace.
			}else if(KeyPress == 0x0A){ // Key Return.
				if(TextMode[Active_Window]==OTHER && TextBuffer[Active_Window][Editor_Cur]=='\n' &&
				 Editor_Insert && Editor_RetMode==OTHER){
					ins1= 2, ins2=-1, ins3=1, ins4=-1, ins5=1;
				}else if(TextMode[Active_Window]==OTHER && TextBuffer[Active_Window][Editor_Cur]=='\n' &&
				 Editor_Insert){
					ins1= 1, ins2=-2, ins3=2, ins4=-1, ins5=0;
				}else if(TextMode[Active_Window]==OTHER && TextBuffer[Active_Window][Editor_Cur]=='\n' &&
				 Editor_RetMode==OTHER){
					ins1= 0, ins2=-1, ins3= 1, ins4=1, ins5=1;
				}else if(TextMode[Active_Window]==OTHER && TextBuffer[Active_Window][Editor_Cur]=='\n'){
					ins1= 0, ins2=-2, ins3=0, ins4=1, ins5=0;
				}else if(TextMode[Active_Window]==OTHER && TextBuffer[Active_Window][Editor_Cur+1]=='\n' &&
				 Editor_RetMode==OTHER){
					ins1= 1, ins2= 0, ins3= 2, ins4=2, ins5=2;
				}else if(TextMode[Active_Window]==OTHER && TextBuffer[Active_Window][Editor_Cur+1]=='\n'){
					ins1= 0, ins2=-1, ins3= 1, ins4=2, ins5=1;
				}else if((Editor_Insert || TextBuffer[Active_Window][Editor_Cur]=='\0') && Editor_RetMode==OTHER){
					ins1= 2, ins2= 0, ins3= 2, ins4=0, ins5=2;
				}else if(Editor_Insert || TextBuffer[Active_Window][Editor_Cur]=='\0'){
					ins1= 1, ins2=-1, ins3= 1, ins4=0, ins5=1;
				}else if(Editor_RetMode==OTHER){
					ins1= 1, ins2= 0, ins3= 2, ins4=1, ins5=2;
				}else{
					ins1= 0, ins2=-1, ins3= 1, ins4=1, ins5=1;
				}
				Operation=2;
				//ends Key Return.
			}else{ // All Other Keys.
				if(TextMode[Active_Window]==OTHER && TextBuffer[Active_Window][Editor_Cur]=='\n'){
					ins1=0, ins2=-1, ins3=0, ins4= 1, ins5= 0;
				}else if(TextMode[Active_Window]==OTHER && TextBuffer[Active_Window][Editor_Cur+1]=='\n' && !Editor_Insert){
					ins1=0, ins2= 0, ins3=1, ins4= 2, ins5= 1;
				}else if(TextMode[Active_Window]==OTHER && TextBuffer[Active_Window][Editor_Cur]=='\n' && Editor_Insert){
					ins1=1, ins2=-1, ins3=0, ins4=-1, ins5=-1;
				}else if(Editor_Insert || TextBuffer[Active_Window][Editor_Cur]=='\0'){
					ins1=1, ins2= 0, ins3=1, ins4= 0, ins5= 1;
				}else{
					ins1=0, ins2= 0, ins3=1, ins4= 1, ins5= 1;
				}
				Operation=3;
			}
		}
		if(Operation>0){ // Add Char / Paste. Can Be Simplify???
			TextBuffer[TMP] = malloc(TextSize[Active_Window]+ins1+256); // 256 To Avoid Crash 256???
			strcpy(TextBuffer[TMP], TextBuffer[Active_Window]);
			memset(TextBuffer[Active_Window], 0, TextSize[Active_Window]+256); // 256 To Avoid Crash 256???
			free(TextBuffer[Active_Window]);
			TextBuffer[Active_Window] = malloc(TextSize[Active_Window]+ins1+256); // 256 To Avoid Crash 256???
			strcpy(TextBuffer[Active_Window], TextBuffer[TMP]);
			if(Operation==1){ // Paste.
				for(i=0; i<ins2; i++)
					TextBuffer[Active_Window][i+Editor_Cur]=TextBuffer[EDIT][i];
			}else if(Operation==2){ // Return.
				if(Editor_RetMode==OTHER){
					TextBuffer[Active_Window][Editor_Cur+ins2]='\r';
					TextBuffer[Active_Window][Editor_Cur+ins2+1]='\n';
				}else if(Editor_RetMode==UNIX){
					TextBuffer[Active_Window][Editor_Cur+ins2+1]='\r';
				}else if(Editor_RetMode==MAC){
					TextBuffer[Active_Window][Editor_Cur+ins2+1]='\n';
				}
			}else if(Operation==3) // Any Other Char.
				TextBuffer[Active_Window][Editor_Cur+ins2]=KeyPress;
			TextBuffer[Active_Window][Editor_Cur+ins3]='\0';
			strcat(TextBuffer[Active_Window], &TextBuffer[TMP][Editor_Cur+ins4]);
			Editor_Cur+=ins5, TextSize[Active_Window]+=ins1;
			memset(TextBuffer[TMP], 0, TextSize[Active_Window]+256); // 256 To Avoid Crash 256???
			free(TextBuffer[TMP]);
			t=0;
		}else if(Operation<0){ // Del Char / Cut. Can Be Simplify???
			TextBuffer[TMP] = malloc(TextSize[Active_Window]+256); // 256 To Avoid Crash 256???
			strcpy(TextBuffer[TMP], TextBuffer[Active_Window]);
			TextBuffer[Active_Window][Editor_Cur+del1]='\0';
			strcat(TextBuffer[Active_Window], &TextBuffer[TMP][Editor_Cur+del2]);
			strcpy(TextBuffer[TMP], TextBuffer[Active_Window]);
			memset(TextBuffer[Active_Window], 0, TextSize[Active_Window]+256); // 256 To Avoid Crash 256???
			free(TextBuffer[Active_Window]);
			TextBuffer[Active_Window] = malloc(TextSize[Active_Window]+del3+256); // 256 To Avoid Crash 256???
			strcpy(TextBuffer[Active_Window], TextBuffer[TMP]);
			Editor_Cur+=del3, TextSize[Active_Window]+=del4;
			memset(TextBuffer[TMP], 0, TextSize[Active_Window]+256); // 256 To Avoid Crash 256???
			free(TextBuffer[TMP]);
			t=0;
		}
abort:
		Operation=0;
		KeyPress = '\0';
	} //ends if(PS2KbdRead(&KeyPress)).
	
	return ret;
}
//------------------------------
//endfunc KeyBoard_Entry
//--------------------------------------------------------------
void Editor_Rules(void)
{
	int i;

	Editor_nChar=TextSize[Active_Window]+1;

	Top_Height=0, Top_Width =0;
	Editor_nRowsNum=0, Editor_nCharRows=1, Editor_nRowsTop=1;
	for(i=0; i<Editor_nChar; i++) // Rows Number, Width, Top, Calucations.
	{
		if((TextMode[Active_Window]==UNIX && TextBuffer[Active_Window][i]=='\r') || // Text Mode UNIX End Line.
		   TextBuffer[Active_Window][i]=='\n' || // Text Mode MAC Or OTHER End Line.
		   Editor_nCharRows>=Rows_Width ||       // Line Width > Screen Width.
		   TextBuffer[Active_Window][i]=='\0'){  // End Text.
			if(i<Editor_Cur){
				if((Editor_nRowsTop += 1)>Rows_Num){
					Top_Width  += Editor_nRowsWidth[Top_Height];
					Top_Height += 1;
				}
			}
			Editor_nRowsWidth[Editor_nRowsNum]=Editor_nCharRows;
			Editor_nRowsNum++;
			Editor_nCharRows=0;
		}
		if(TextBuffer[Active_Window][i]=='\0') // End Text Stop Calculations.
			break;
		Editor_nCharRows++;
	}

	if(Editor_Home){
		Tmp_Cur=0;
		for(i=0; i<Editor_nRowsNum; i++) // Home Rules.
		{
			if((Tmp_Cur += Editor_nRowsWidth[i])>=Editor_Cur+1)
				break;
		}
		Tmp_Cur -= (Editor_nRowsWidth[i]-1);
		Editor_Cur = Tmp_Cur-1;
		Editor_Home=0;
	}

	if(Editor_End){
		Tmp_Cur=0;
		for(i=0; i<Editor_nRowsNum; i++) // End Rules.
		{
			if((Tmp_Cur += Editor_nRowsWidth[i])>=Editor_Cur+1)
				break;
		}
		Editor_Cur = Tmp_Cur-1;
		Editor_End=0;
	}

	if(Editor_PushRows<0){
		Tmp_Cur=0;
		for(i=0; i<Editor_nRowsNum; i++) // Line / Page Up Rules.
		{
			if((Tmp_Cur += Editor_nRowsWidth[i])>=Editor_Cur+1)
				break;
		}
		Tmp_Cur -= (Editor_nRowsWidth[i]-1);
		if(Editor_nRowsWidth[i+1]<=((Editor_Cur+1)-(Tmp_Cur-1)))
			Editor_Cur = Tmp_Cur+Editor_nRowsWidth[i]+Editor_nRowsWidth[i+1]-1-1;
		else
			Editor_Cur = Tmp_Cur+Editor_nRowsWidth[i]+((Editor_Cur+1)-Tmp_Cur)-1;

		if(Editor_PushRows++ >= 0)
			Editor_PushRows=0;
		
		if(Editor_Cur+1>=Editor_nChar && TextBuffer[Active_Window][Editor_Cur]!='\0'){
			Editor_Cur=Editor_nChar-1;
			Editor_PushRows=0;
		}
	}

	if(Editor_PushRows>0){
		Tmp_Cur=0;
		for(i=0; i<Editor_nRowsNum; i++) // Line / Page Down Rules.
		{
			if((Tmp_Cur += Editor_nRowsWidth[i])>=Editor_Cur+1)
				break;
		}
		Tmp_Cur -= (Editor_nRowsWidth[i]-1);

		if(Editor_nRowsWidth[i-1]<=((Editor_Cur+1)-(Tmp_Cur-1)))
			Editor_Cur = Tmp_Cur-1-1;
		else
			Editor_Cur = Tmp_Cur-Editor_nRowsWidth[i-1]+((Editor_Cur+1)-Tmp_Cur)-1;

		if(Editor_PushRows-- <= 0)
			Editor_PushRows=0;

		if(Editor_Cur<=0){
			Editor_Cur=0;
			Editor_PushRows=0;
		}
	}

	if(Editor_Cur >= Editor_nChar) // Max Char Number Rules.
		Editor_Cur=Editor_nChar-1;

	if(Editor_Cur < 0) // Min Char Number Rules.
		Editor_Cur=0;

	if(Mark[MARK_ON]){ // Mark Rules.
		Mark[MARK_SIZE]=Editor_Cur-Mark[MARK_IN];
		if(Mark[MARK_SIZE]>0){
			Mark[MARK_PRINT]=Mark[MARK_SIZE];
			if(Mark[MARK_IN]<Top_Width)
				Mark[MARK_PRINT]=Editor_Cur-Top_Width;
		}else if(Mark[MARK_SIZE]<0)
			Mark[MARK_PRINT]=1;
		else
			Mark[MARK_PRINT]=0;
	}

}
//--------------------------------------------------------------
int Windows_Selector(void)
{
	uint64 color;
	int x, y, i, Window_Sel=Active_Window;
	int event, post_event=0;

	int Window_ch_w = 36;           //Total characters in longest Window Name.
	int Window_ch_h = 10;   //Total number of Window Menu lines.
	int wSprite_Y1 = 100;           //Top edge of sprite.
	int wSprite_X2 = SCREEN_WIDTH-35;   //Right edge of sprite.
	int wFrame_Y1 = wSprite_Y1;  //Top edge of frame.
	int wFrame_X2 = wSprite_X2-3;  //Right edge of frame (-3 correct ???).
	int wFrame_X1 = wFrame_X2-(Window_ch_w+3)*FONT_WIDTH;    //Left edge of frame.
	int wFrame_Y2 = wFrame_Y1+(Window_ch_h+1)*FONT_HEIGHT/2; //Bottom edge of frame.
	int wSprite_X1 = wFrame_X1-1;  //Left edge of sprite.
	int wSprite_Y2 = wFrame_Y2;  //Bottom edge of sprite.

	event = 1;  //event = initial entry.
	while(1){
		//Pad response section.
		waitPadReady(0, 0);
		if(readpad()){
			if(new_pad & PAD_UP){
				event |= 2;  //event |= valid pad command.
				if((Window_Sel--)<=0)
					Window_Sel=9;
			}else if(new_pad & PAD_DOWN){
				event |= 2;  //event |= valid pad command.
				if((Window_Sel++)>=9)
					Window_Sel=0;
			}else if((new_pad & PAD_TRIANGLE)
						|| (!swapKeys && new_pad & PAD_CROSS)
			      || (swapKeys && new_pad & PAD_CIRCLE) ){
				return -1;
			}else if((swapKeys && new_pad & PAD_CROSS)
			      || (!swapKeys && new_pad & PAD_CIRCLE) ){
				event |= 2;  //event |= valid pad command.
				break;
			}
		}

		if(event||post_event){ //NB: We need to update two frame buffers per event.

			//Display section.
			drawPopSprite(setting->color[0],
				wSprite_X1, wSprite_Y1,
				wSprite_X2, wSprite_Y2);
			drawFrame(wFrame_X1, wFrame_Y1, wFrame_X2, wFrame_Y2, setting->color[1]);

			for(i=0,y=wFrame_Y1*2+FONT_HEIGHT/2; i<10; i++){
				if(Window_Sel==i)
					color = setting->color[2];
				else
					color = setting->color[3];

				if(!Window[i][OPENED])
					printXY("Free Window", wFrame_X1+2*FONT_WIDTH, y/2, color, TRUE);
				else if(Window[i][CREATED])
					printXY("Window Not Yet Saved", wFrame_X1+2*FONT_WIDTH, y/2, color, TRUE);
				else if(Window[i][OPENED])				
					printXY(Path[i], wFrame_X1+2*FONT_WIDTH, y/2, color, TRUE);

				y+=FONT_HEIGHT;
			}

			if(Window_Sel<=10)
				drawChar(127, wFrame_X1+FONT_WIDTH, wFrame_Y1+(FONT_HEIGHT/2+Window_Sel*FONT_HEIGHT)/2, setting->color[3]);

			//Tooltip section.
			x = SCREEN_MARGIN;
			y = Menu_tooltip_y;
			drawSprite(setting->color[0],
				0, (y/2),
				SCREEN_WIDTH, y/2+10);
			if (swapKeys)
				printXY("ÿ1:OK ÿ0:Cancel ÿ3:Back", x, y/2, setting->color[2], TRUE);
			else
				printXY("ÿ0:OK ÿ1:Cancel ÿ3:Back", x, y/2, setting->color[2], TRUE);
		}//ends if(event||post_event).
		drawScr();
		post_event = event;
		event = 0;
	}//ends while.
	return Window_Sel;
}//ends Window_Selector.
//--------------------------------------------------------------
void Init(void)
{
	int i;

	for(i=0; i<MAX_ENTRY; i++)
		Editor_nRowsWidth[i]=0;

	Editor_Cur=0, Tmp_Cur=0,
	Editor_nChar=0, Editor_nRowsNum=0,
	Editor_nCharRows=0, Editor_nRowsTop=0,
	Editor_PushRows=0, Editor_TextEnd=0;

	Top_Width=0, Top_Height=0;

	Rows_Width  = (SCREEN_WIDTH-SCREEN_MARGIN-LINE_THICKNESS-28-Menu_start_x)/FONT_WIDTH;
	Rows_Num = (Menu_end_y-Menu_start_y)/FONT_HEIGHT;

	KeyBoard_Cur=2, KeyBoard_Active=0,
	Editor_Insert=1, Editor_RetMode=TextMode[Active_Window];
	Editor_Home=0, Editor_End=0;

	del1=0, del2=0, del3=0, del4=0;
	ins1=0, ins2=0, ins3=0, ins4=0, ins5=0;

	if(Mark[MARK_ON]){
		Mark[MARK_ON]=0, Mark[MARK_IN]=0,
		Mark[MARK_OUT]=0, Mark[MARK_TMP]=0,
		Mark[MARK_PRINT]=0, Mark[MARK_COLOR]=0;
	}

}
//--------------------------------------------------------------
int New(int Win)
{
	int i, ret=0;

	TextSize[Win]=1;
	
	if( TextSize[Win] ){
		if( ( TextBuffer[Win] = malloc( TextSize[Win]+256 ) ) > 0 ){
			TextBuffer[Win][0]='\0';
			TextMode[Win]=OTHER;
			Window[Win][CREATED]=1, Window[Win][OPENED]=1, Window[Win][SAVED]=0;
			Init();
			ret=1;
		}
	}

	if(ret){
		drawMsg("File Created");
	}else{
		drawMsg("Failed Creating File");
	}

	i=0;
	while(i<40000000) // print operation result during 2 sec.
	 i++;

	return ret;
}
//--------------------------------------------------------------
int Open(int Win)
{
	int fd, i, ret=0;
	char filePath[MAX_PATH];

	getFilePath(Path[Win], TEXT_CNF); // No Filtering Be Carreful.

	if(Path[Win][0]=='\0')
		goto abort;

	genFixPath(Path[Win], filePath);
	fd = genOpen( filePath, O_RDONLY );
 
	if( fd >= 0 ) {
		TextSize[Win] = genLseek ( fd, 0, SEEK_END );
		genLseek ( fd, 0, SEEK_SET );

		if( TextSize[Win] && TextSize[Win] <= 512*1024 ){ // Limit Text Size To 512Kb???
			if( ( TextBuffer[Win] = malloc( TextSize[Win]+256 ) ) > 0 ){ // 256 To Avoid Crash 256???
				memset(TextBuffer[Win], 0, TextSize[Win]+256); // 256 To Avoid Crash 256???
				genRead( fd, TextBuffer[Win], TextSize[Win] );

				for(i=0; i<TextSize[Win]; i++){ // Scan For Text Mode.
					if(TextBuffer[Win][i-1]!='\r' && TextBuffer[Win][i]=='\n'){ // Mode MAC Only LF At Line End.
						TextMode[Win]=MAC;
						break;
					}else if(TextBuffer[Win][i]=='\r' && TextBuffer[Win][i+1]=='\n'){ // Mode OTHER CR/LF At Line End.
						TextMode[Win]=OTHER;
						break;
					}else if(TextBuffer[Win][i]=='\r' && TextBuffer[Win][i+1]!='\n'){ // Mode UNIX Only CR At Line End.
						TextMode[Win]=UNIX;
						break;
					}
				}

				Window[Win][OPENED]=1, Window[Win][SAVED]=0;
				Init();
				ret=1;
			}
		}
	}

	genClose( fd );

	if(ret){
		drawMsg("File Opened");
	}else{
abort:
		TextSize[Win]=0;
		drawMsg("Failed Opening File");
	}

	i=0;
	while(i<40000000) // print operation result during 2 sec.
	 i++;

	return ret;
}
//--------------------------------------------------------------
void Close(int Win)
{
	int i;
	char msg[MAX_PATH];

	memset(TextBuffer[Win], 0, TextSize[Win]+256); // 256 To Avoid Crash 256???
	free(TextBuffer[Win]);

	if(Window[Win][CREATED])
		strcpy(msg, "File Not Yet Saved Closed");
	else
		sprintf(msg, "File %s Closed", Path[Win]);

	Path[Win][0]='\0';

	TextMode[Win]=0, TextSize[Win]=0, Window[Win][CREATED]=0, Window[Win][OPENED]=0, Window[Win][SAVED]=1;

	Init();

	drawMsg(msg);

	i=0;
	while(i<40000000) // print operation result during 2 sec.
	 i++;
}
//--------------------------------------------------------------
void Save(int Win)
{
	int fd, i, ret=0;

	char filePath[MAX_PATH];

	if(!strncmp(Path[Win], "cdfs", 4))
		goto abort;
	genFixPath(Path[Win], filePath);

	fd = genOpen( filePath, O_CREAT|O_WRONLY|O_TRUNC );
 
	if( fd >= 0 ) {
		if(TextMode[Win]==OTHER && TextBuffer[Win][TextSize[Win]]=='\n')
			genWrite(fd, TextBuffer[Win], TextSize[Win]-1);
		else
			genWrite(fd, TextBuffer[Win], TextSize[Win]);
		Window[Win][OPENED]=1, Window[Win][SAVED]=1;
		ret=1;
	}
	
	genClose( fd );
	if(!strncmp(filePath, "pfs", 3))
		unmountParty(filePath[3]-'0');

	if(ret){
		drawMsg("File Saved");
	}else{
abort:
		drawMsg("Failed Saving File");
	}

	i=0;
	while(i<40000000) // print operation result during 2 sec.
	 i++;
}
//--------------------------------------------------------------
void Save_As(int Win)
{
	int fd, i, ret=0;
	char tmp[MAX_PATH], oldPath[MAX_PATH], filePath[MAX_PATH];
	char *p;

	tmp[0]='\0', oldPath[0]='\0', filePath[0]='\0';

	if(Path[Win][0]!='\0'){
		strcpy(oldPath, Path[Win]);
		Path[Win][0]='\0';
		p=strrchr(oldPath, '/');
		strcpy(tmp, p+1);
	}

	getFilePath(Path[Win], DIR_CNF);
	if(Path[0] == '\0')
		goto abort;

	drawMsg("Enter File Name.");
	drawMsg("Enter File Name.");

	if(keyboard(tmp, 36)>0)
		strcat(Path[Win], tmp);
	else
		goto abort;

	if(!strncmp(Path[Win], "cdfs", 4))
		goto abort;
	genFixPath(Path[Win], filePath);

	fd = genOpen( filePath, O_CREAT|O_WRONLY|O_TRUNC );
 
	if( fd >= 0 ) {
		if(TextMode[Win]==OTHER && TextBuffer[Win][TextSize[Win]]=='\n')
			genWrite(fd, TextBuffer[Win], TextSize[Win]-1);
		else
			genWrite(fd, TextBuffer[Win], TextSize[Win]);
		Window[Win][CREATED]=0, Window[Win][OPENED]=1, Window[Win][SAVED]=1;
		ret=1;
	}
	
	genClose( fd );
	if(!strncmp(filePath, "pfs", 3))
		unmountParty(filePath[3]-'0');

	if(ret){
		drawMsg("File Saved");
	}else{
abort:
		if(oldPath[0]!='\0')
			strcpy(Path[Win], oldPath);
		drawMsg("Failed Saving File");
	}

	i=0;
	while(i<40000000) // print operation result during 2 sec.
	 i++;
}
//--------------------------------------------------------------
void TextEditor(void)
{
	char   tmp[MAX_PATH], tmp1[MAX_PATH], c[2];
	int    x, y, y0, y1;
	int    i=0, j, ret=0;
	int    tmpLen=0;
	int    event=1, post_event=0;
	int    Editor_Start=0;
	uint64 color;
	const int	KEY_W=350,
				KEY_H=58,  //NB: This is not real height, but height/2.
				KEY_X=(SCREEN_WIDTH - KEY_W)/2,
				KEY_Y=(Menu_end_y - 2*KEY_H)/2;
	int KEY_LEN = strlen(KEY);
	
	tmp[0]='\0', tmp1[0]='\0', c[0]='\0';
	
	Active_Window=0, Num_Window=0;

	for(i=0; i<10; i++){
		Window[i][CREATED]=0;
		Window[i][OPENED]=0;
		Window[i][SAVED]=1;
		TextMode[i]=0;
		TextSize[i]=0;
		Path[i][0]='\0';
	}

	for(i=0; i<NUM_MARK; i++)
		Mark[i]=0;

	Init();
	
	t=0;

	event = 1;  //event = initial entry.

	Rows_Width  = (SCREEN_WIDTH-SCREEN_MARGIN-LINE_THICKNESS-28-Menu_start_x)/FONT_WIDTH;
	Rows_Num = (Menu_end_y-Menu_start_y)/FONT_HEIGHT;

	x = Menu_start_x;
	y = Menu_start_y;
	
	while(1){

		//Pad response section.
		waitPadReady(0, 0);
		if(readpad_no_KB()){
			if(new_pad){
				event |= 2;  //event |= pad command.
			}
			if(!KeyBoard_Active){ // Pad Response Without KeyBoard.
				if(new_pad & PAD_UP){ // Text move up.
					if(Editor_Cur>0)
						Editor_PushRows++;
				}else if(new_pad & PAD_DOWN){ // Text move down.
					if(Editor_Cur<Editor_nChar && TextBuffer[Active_Window][Editor_Cur]!='\0')
						Editor_PushRows--;
				}else if(new_pad & PAD_LEFT){ // Text move left.
					if(Editor_Cur>0)
						Editor_Cur--;
				}else if(new_pad & PAD_RIGHT){ // Text move right.
					if(Editor_Cur<Editor_nChar && TextBuffer[Active_Window][Editor_Cur]!='\0')
						Editor_Cur++;
				}else if(new_pad & PAD_L2){ // Text move page up.
					if(Editor_Cur>0)
						Editor_PushRows += 2*(Rows_Num-1);
				}else if(new_pad & PAD_R2){ // Text move page down.
					if(Editor_Cur<Editor_nChar && TextBuffer[Active_Window][Editor_Cur]!='\0')
						Editor_PushRows -= 2*(Rows_Num-1);
				}else if(new_pad & PAD_SELECT && Window[Active_Window][OPENED]){ // Virtual KeyBoard Active Rows_Num -= 7.
						KeyBoard_Cur=2;
						Rows_Num -= 6;
						KeyBoard_Active = 1;
				}
			}else{ // Pad Response With Virtual KeyBoard.
				Virt_KeyBoard_Entry();
			}

			if(new_pad & PAD_TRIANGLE){ // General Pad Response.
exit:
				drawMsg("Exiting Editor.");
				for(i=0; i<10; i++){
					if(!Window[i][SAVED])
						goto unsave;
				}
force:
				for(i=0; i<10; i++){
					if(Window[i][OPENED]){
						Close(i);
						Path[i][0]='\0';
					}
				}
				if(Mark[MARK_COPY] || Mark[MARK_CUT])
					free(TextBuffer[EDIT]);
				Mark[MARK_START]=0, Mark[MARK_ON]=0, Mark[MARK_COPY]=0, Mark[MARK_CUT]=0,
				Mark[MARK_IN]=0, Mark[MARK_OUT]=0, Mark[MARK_TMP]=0,
				Mark[MARK_SIZE]=0, Mark[MARK_PRINT]=0, Mark[MARK_COLOR]=0;

				return;
unsave:
				if(ynDialog("Exit Without Saving ?")!=1)
					goto abort;
				else
					goto force;
			}else if(new_pad & PAD_R1) {
menu:
				ret = MenuEditor();
				if(ret==NEW){
					Num_Window=0;
					for(i=0; i<10; i++){
						if(Window[i][OPENED])
							Num_Window++;
					}
					if(Num_Window<10){
						for(i=0; i<10; i++){
							if(!Window[i][OPENED]){
								Active_Window=i;
								break;
							}
						}
						ret=New(Active_Window);
						if(!ret)
							goto fail;
						Editor_Cur=0, Editor_PushRows=0;
					}
					Num_Window=0;
					for(i=0; i<10; i++){
						if(Window[i][OPENED])
							Num_Window++;
					}
				} else if(ret==OPEN){
					drawMsg("Select A File For Editing.");
					drawMsg("Select A File For Editing.");
					Num_Window=0;
					for(i=0; i<10; i++){
						if(Window[i][OPENED])
							Num_Window++;
					}
					if(Num_Window<10){
						for(i=0; i<10; i++){
							if(!Window[i][OPENED]){
								Active_Window=i;
								break;
							}
						}
						ret=Open(Active_Window);
						if(!ret)
							goto fail;
						Editor_Cur=0, Editor_PushRows=0;
					}
					Num_Window=0;
					for(i=0; i<10; i++){
						if(Window[i][OPENED])
							Num_Window++;
					}
				} else if(ret==CLOSE){
					if(!Window[Active_Window][SAVED]){
						if(ynDialog("Close Without Saving ?")!=1)
							goto abort;
					}
					Close(Active_Window);
fail:
					for(i=9; i>-1; i--){
						if(Window[i][OPENED]){
							Active_Window=i;
							break;
						}
					}
					Num_Window=0;
					for(i=0; i<10; i++){
						if(Window[i][OPENED])
							Num_Window++;
					}
abort:
					i=0; // just for compiler warning.
				} else if(ret==SAVE){
					Save(Active_Window);
				} else if(ret==SAVE_AS){
					Save_As(Active_Window);
				} else if(ret==WINDOWS){
					ret=Windows_Selector();
					if(ret>=0){
						Active_Window=ret;
						Init();
					}
				} else if(ret==EXIT){
					goto exit;
				}
			}
		}//ends pad response section.

		if(!Num_Window)
			Editor_Start++;
		if(Editor_Start==4){
			Editor_Start=0;
			event |= 2;
			goto menu;
		}
		
		if(setting->usbkbd_used){	// Kbd response section.
			
			ret = KeyBoard_Entry();
			if(ret)
				event |=2;
			if(ret==2)
				goto menu;
			else if(ret==3)
				goto exit;

		}	// end Kbd response section.

		t++;

		if(t & 0x0F) event |= 4;  //repetitive timer event.

		if(event||post_event){ //NB: We need to update two frame buffers per event.

			//Display section.
			clrScr(setting->color[0]);
			
			if(TextSize[Active_Window]==0)
				goto end;

			itoSprite(COL_NORM_BG,
				SCREEN_MARGIN, Frame_start_y/2,
				SCREEN_WIDTH-SCREEN_MARGIN, Frame_end_y/2,
				0);

			if(KeyBoard_Active){ //Display Virtual KeyBoard Section.

				drawPopSprite(setting->color[0],
					SCREEN_MARGIN, KEY_Y+11,
					SCREEN_WIDTH-SCREEN_MARGIN, Frame_end_y/2);

				itoLine(setting->color[1], SCREEN_MARGIN, KEY_Y+11, 0,
				setting->color[1], SCREEN_WIDTH-SCREEN_MARGIN, KEY_Y+11, 0);
				itoLine(setting->color[1], KEY_X-64, KEY_Y+11, 0,
				setting->color[1], KEY_X-64, Frame_end_y/2, 0);
				itoLine(setting->color[1], KEY_X+32, KEY_Y+11, 0,
				setting->color[1], KEY_X+32, Frame_end_y/2, 0);
				itoLine(setting->color[1], KEY_X+KEY_W+32, KEY_Y+11, 0,
				setting->color[1], KEY_X+KEY_W+32, Frame_end_y/2, 0);

				if(Mark[MARK_ON])
					color=setting->color[2];
				else
					color=setting->color[3];
				printXY("MARK",
					KEY_X+2+4 - 120, KEY_Y+FONT_HEIGHT, color, TRUE);
				printXY("        LINE UP",
					KEY_X+2+4 - 120, KEY_Y+FONT_HEIGHT, setting->color[3], TRUE);
				if(Mark[MARK_COPY])
					color=setting->color[2];
				else
					color=setting->color[3];
				printXY("COPY",
					KEY_X+2+4 - 120, KEY_Y+FONT_HEIGHT+FONT_HEIGHT/2, color, TRUE);
				printXY("        LINE DOWN",
					KEY_X+2+4 - 120, KEY_Y+FONT_HEIGHT+FONT_HEIGHT/2, setting->color[3], TRUE);
				if(Mark[MARK_CUT])
					color=setting->color[2];
				else
					color=setting->color[3];
				printXY("CUT",
					KEY_X+2+4 - 120, KEY_Y+FONT_HEIGHT*2, color, TRUE);
				printXY("        PAGE UP",
					KEY_X+2+4 - 120, KEY_Y+FONT_HEIGHT*2, setting->color[3], TRUE);
				printXY("PASTE   PAGE DOWN",
					KEY_X+2+4 - 120, KEY_Y+FONT_HEIGHT*2+FONT_HEIGHT/2, setting->color[3], TRUE);
				printXY("HOME    END",
					KEY_X+2+4 - 120, KEY_Y+FONT_HEIGHT*3, setting->color[3], TRUE);

				if(Editor_Insert)
					color=setting->color[2];
				else
					color=setting->color[3];
				printXY("INSERT",
					KEY_X+2+4 + 392, KEY_Y+FONT_HEIGHT, color, TRUE);
				tmp[0]='\0';
				if(Editor_RetMode==OTHER)
					strcpy(tmp, "RET CR/LF");
				else if(Editor_RetMode==UNIX)
					strcpy(tmp, "RET CR");
				else if(Editor_RetMode==MAC)
					strcpy(tmp, "RET LF");
				printXY(tmp,
					KEY_X+2+4 + 392, KEY_Y+FONT_HEIGHT+FONT_HEIGHT/2, setting->color[3], TRUE);
				printXY("TAB",
					KEY_X+2+4 + 392, KEY_Y+FONT_HEIGHT*2, setting->color[3], TRUE);
				printXY("SPACE",
					KEY_X+2+4 + 392, KEY_Y+FONT_HEIGHT*2+FONT_HEIGHT/2, setting->color[3], TRUE);
				printXY("RETURN",
					KEY_X+2+4 + 392, KEY_Y+FONT_HEIGHT*3, setting->color[3], TRUE);

				for(i=0; i<KEY_LEN; i++){
					drawChar(KEY[i],
						KEY_X+2+4+14 + (i%WFONTS+1)*20 - 32,
						KEY_Y+16 + (i/WFONTS)*8,
						setting->color[3]);
				}

				//Virtual KeyBoard Cursor positioning section.
				if(!KeyBoard_Cur || KeyBoard_Cur%WFONTS==0)
					x = KEY_X+2+4 - 128;
				else if(KeyBoard_Cur==1 || (KeyBoard_Cur-1)%WFONTS==0)
					x = KEY_X+2+4 - 64;
				else if((KeyBoard_Cur+1)%WFONTS==0)
					x = KEY_X+2+4 + 384;
				else
					x = KEY_X+2+4+14 + (KeyBoard_Cur%WFONTS+1)*20 - 40;
				y = KEY_Y+16 + (KeyBoard_Cur/WFONTS)*8;
				drawChar(127, x, y, setting->color[2]);
			
			} // end Display Virtual KeyBoard Section.

			x = Menu_start_x;
			y = Menu_start_y;

			Editor_Rules();

			Editor_TextEnd=0, tmpLen=0;

			for(i=Top_Height; i<Rows_Num+Top_Height; i++)
			{
				for(j=0; j<Editor_nRowsWidth[i]; j++)
				{
					Mark[MARK_COLOR]=0;

					if(Mark[MARK_ON] && Mark[MARK_PRINT]>0){ //Mark Text.
						if(Mark[MARK_SIZE]>0){
							if(Top_Width+tmpLen+j == (Editor_Cur-Mark[MARK_PRINT])){
								itoSprite(COL_MARK_BG, x,y/2,x+8,y/2+8, 0);
								Mark[MARK_COLOR]=1;
								Mark[MARK_PRINT]--;
							}
						}else if(Mark[MARK_SIZE]<0){
							if(Top_Width+tmpLen+j == (Editor_Cur+Mark[MARK_PRINT]-1)){
								itoSprite(COL_MARK_BG, x,y/2,x+8,y/2+8, 0);
								Mark[MARK_COLOR]=1;
								if((Mark[MARK_PRINT]++) == (-Mark[MARK_SIZE]))
									Mark[MARK_PRINT]=0;
							}
						}
					} // end mark.

					if(Top_Width+tmpLen+j == Editor_Cur){ //Text Cursor.
						if(Editor_Insert)
							color = COL_CUR_INSERT;
						else
							color = COL_CUR_OVERWR;
						if(((event|post_event)&4) && (t & 0x10))
							printXY("|", x-4, y/2, color, TRUE);
					}

					if(TextBuffer[Active_Window][Top_Width+tmpLen+j]=='\n'){ // Line Feed.
						c[0]='';
						color = COL_LINE_END;
					}else if (TextBuffer[Active_Window][Top_Width+tmpLen+j]=='\r'){ // Carriage Return.
						c[0]='ž';
						color = COL_LINE_END;
					}else if (TextBuffer[Active_Window][Top_Width+tmpLen+j]=='\t'){ // Tabulation.
						c[0]='Ÿ';
						color = COL_TAB;
					}else if (TextBuffer[Active_Window][Top_Width+tmpLen+j]=='\0'){ // Text End.
						c[0]='ˆ';
						color = COL_TEXT_END;
						Editor_TextEnd=1;
					}else{
						c[0]=TextBuffer[Active_Window][Top_Width+tmpLen+j];
						if(Mark[MARK_ON] && Mark[MARK_COLOR]) //Text Color Black / White If Mark.
							color = COL_MARK_TEXT;
						else
							color = COL_NORM_TEXT;
					}

					c[1]='\0';
					printXY(c, x, y/2, color, TRUE);

					if(Editor_TextEnd)
						goto end;
					
					x += FONT_WIDTH;
				}

				tmpLen += Editor_nRowsWidth[i];

				x = Menu_start_x;
				y += FONT_HEIGHT;

			} //ends for, so all editor Rows_Num were fixed above.
end:
			if(Editor_nRowsNum > Rows_Num) { //if more lines than available Rows_Num, use scrollbar.
				if(KeyBoard_Active){
					drawFrame(SCREEN_WIDTH-SCREEN_MARGIN-LINE_THICKNESS-15, Frame_start_y/2,
						SCREEN_WIDTH-SCREEN_MARGIN, KEY_Y+11, setting->color[1]);
					y0=(((KEY_Y+11)*2-4)-Menu_start_y+8) * ((double)Top_Height/Editor_nRowsNum);
					y1=(((KEY_Y+11)*2-4)-Menu_start_y+8) * ((double)(Top_Height+Rows_Num)/Editor_nRowsNum);
					itoSprite(setting->color[1],
						SCREEN_WIDTH-SCREEN_MARGIN-LINE_THICKNESS-11,
						(y0+Menu_start_y-4)/2,
						SCREEN_WIDTH-SCREEN_MARGIN-LINE_THICKNESS-1,
						(y1+Menu_start_y-4)/2,
						0);
				}else{
					drawFrame(SCREEN_WIDTH-SCREEN_MARGIN-LINE_THICKNESS-15, Frame_start_y/2,
						SCREEN_WIDTH-SCREEN_MARGIN, Frame_end_y/2, setting->color[1]);
					y0=(Menu_end_y-Menu_start_y+8) * ((double)Top_Height/Editor_nRowsNum);
					y1=(Menu_end_y-Menu_start_y+8) * ((double)(Top_Height+Rows_Num)/Editor_nRowsNum);
					itoSprite(setting->color[1],
						SCREEN_WIDTH-SCREEN_MARGIN-LINE_THICKNESS-11,
						(y0+Menu_start_y-4)/2,
						SCREEN_WIDTH-SCREEN_MARGIN-LINE_THICKNESS-1,
						(y1+Menu_start_y-4)/2,
						0);
				} //ends clause for scrollbar with KeyBoard.
			} //ends clause for scrollbar.

			//Tooltip section.
			tmp[0]='\0', tmp1[0]='\0';
			if(KeyBoard_Active){ //Display Virtual KeyBoard Tooltip.
				if(swapKeys) 
					strcpy(tmp1, "R1:Menu ÿ3:Exit ÿ1:Sel ÿ0:BackSpace L2:Left R2:Right SEL:Close KB");
				else
					strcpy(tmp1, "R1:Menu ÿ3:Exit ÿ0:Sel ÿ1:BackSpace L2:Left R2:Right SEL:Close KB");
			}else if(setting->usbkbd_used){ //Display KeyBoard Tooltip.
				if(Window[Active_Window][OPENED]){
					if(Mark[MARK_ON])
						strcpy(tmp1, "F1/R1:Menu Esc/ÿ3:Exit Ctrl+ b:Mark On ");
					else
						strcpy(tmp1, "F1/R1:Menu Esc/ÿ3:Exit Ctrl+ b:Mark Off ");
					strcat(tmp1, "x:Cut c:Copy v:Paste ");
					if(Editor_RetMode==OTHER)
						strcat(tmp1, "r:Cr/Lf ");
					else if(Editor_RetMode==UNIX)
						strcat(tmp1, "r:Cr ");
					else if(Editor_RetMode==MAC)
						strcat(tmp1, "r:Lf ");
					if(Editor_Insert)
						strcat(tmp1, "Ins:On");
					else
						strcat(tmp1, "Ins:Off");
				}else
					strcpy(tmp1, "F1/R1:Menu Esc/ÿ3:Exit");
			}else{ //Display Basic Tooltip.
				if(Window[Active_Window][OPENED]) 
					strcpy(tmp1, "R1:Menu ÿ3:Exit SELECT:Open KeyBoard");
				else
					strcpy(tmp1, "R1:Menu ÿ3:Exit");
			}
			if(Window[Active_Window][CREATED])
				strcpy(tmp, "PS2 TEXT EDITOR : File Not Yet Saved");
			else if(Window[Active_Window][OPENED])
				sprintf(tmp, "PS2 TEXT EDITOR : %s", Path[Active_Window]);
			else
				strcpy(tmp, "PS2 TEXT EDITOR");
			setScrTmp(tmp, tmp1);
		}//ends if(event||post_event).
		drawScr();
		post_event = event;
		event = 0;
	}//ends while.
	
	return;
}
//--------------------------------------------------------------
