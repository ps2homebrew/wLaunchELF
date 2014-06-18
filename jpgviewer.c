//--------------------------------------------------------------
//File name:   jpgviewer.c
//--------------------------------------------------------------
#include "launchelf.h"

static char	 msg0[MAX_PATH], msg1[MAX_PATH];
static float fZoomStep, fOffsetX, fOffsetY;
static u32	 nImagePosX, nImagePosY, nImagePosX1, nImagePosY1;
static int   DiapoTime, DiapoTrans, DiapoBegin, DiapoStart, DiapoStop, DiapoSkip, FullScreen;
static int	 Brightness;
static int	 TimeRemain;

#define OFF        1
#define ZOOM       2
#define FOND       3
#define ZOOM_FOND  4

#define LIST       1
#define THUMBNAIL  2

#define NOTLOADED -1
#define BADJPG  	 0
#define LOADED  	 1

//--------------------------------------------------------------
void View_Render( void ) {

	u32 nTmpPosX, nTmpPosY;
	u32 picOffsetX=0, picOffsetY=0;

	nImagePosX = nTmpPosX = SCREEN_WIDTH/8  - (1.5f - fZoomStep) * (SCREEN_WIDTH/4);
	nImagePosY = nTmpPosY = SCREEN_HEIGHT/8 - (1.5f - fZoomStep) * (SCREEN_HEIGHT/4);
	
	if(fZoomStep == 1.0f){
		fOffsetX = 0.0f;
		fOffsetY = 0.0f;
	}else{
		nImagePosX  += fOffsetX * nTmpPosX;
		nImagePosY  += fOffsetY * nTmpPosY;
	}
	if(nImagePosX  <= 0) nImagePosX = 0;
	if(nImagePosY  <= 0) nImagePosY = 0;
  
	nImagePosX1 = SCREEN_WIDTH  - (nTmpPosX*2 - nImagePosX);
	nImagePosY1 = SCREEN_HEIGHT - (nTmpPosY*2 - nImagePosY);
  
	if(nImagePosX1 >= SCREEN_WIDTH ) nImagePosX1 = SCREEN_WIDTH;
	if(nImagePosY1 >= SCREEN_HEIGHT) nImagePosY1 = SCREEN_HEIGHT;
  
	testskin=0;
	clrScr(setting->color[0]);
	itoSetTexture(0,
		SCREEN_WIDTH, ITO_RGB24, log(SCREEN_WIDTH), log(SCREEN_HEIGHT/2));

	if(Brightness!=100){
		itoPrimFog( TRUE );
		itoSetFogValue( 0, 0, 0, Brightness*2.55f );
	}else
		setBrightness(50);

	if(FullScreen){
		itoSprite(ITO_RGBA( 0, 0, 0, 0 ),
			0, 0,
			SCREEN_WIDTH, SCREEN_HEIGHT/2, 0);
		if(picHeight>=SCREEN_HEIGHT)
			picOffsetX=(SCREEN_WIDTH-(SCREEN_HEIGHT*picCoeff))/2;
		else
			picOffsetY=(SCREEN_HEIGHT-(SCREEN_WIDTH/picCoeff))/2;
		itoTextureSprite(ITO_RGBAQ( 0x80, 0x80, 0x80, 0xFF, 0 ),
			picOffsetX, picOffsetY/2,
			nImagePosX, nImagePosY/2,
			SCREEN_WIDTH-picOffsetX, (SCREEN_HEIGHT-picOffsetY)/2,
			nImagePosX1, nImagePosY1/2, 0);
	}else{
		itoSprite(ITO_RGBA( 0, 0, 0, 0 ),
			SCREEN_MARGIN, Frame_start_y/2,
			SCREEN_WIDTH-SCREEN_MARGIN, Frame_end_y/2, 0);
		if(picHeight>=Frame_end_y-Frame_start_y)
			picOffsetX=((SCREEN_WIDTH-SCREEN_MARGIN*2)-((Frame_end_y-Frame_start_y)*picCoeff))/2;
		else
			picOffsetY=((Frame_end_y-Frame_start_y)-(SCREEN_WIDTH-SCREEN_MARGIN*2)/picCoeff)/2;
		itoTextureSprite(ITO_RGBAQ( 0x80, 0x80, 0x80, 0xFF, 0 ),
			SCREEN_MARGIN+picOffsetX,
			(Frame_start_y+picOffsetY)/2,
			nImagePosX, nImagePosY/2,
			SCREEN_WIDTH-SCREEN_MARGIN-picOffsetX,
			(Frame_end_y-picOffsetY)/2,
			nImagePosX1, nImagePosY1/2, 0);
		setBrightness(50);
		//Tooltip section
		if (swapKeys)
			sprintf(msg1, "ÿ1:FullScreen Dir:Move L1/R1:Zoom Start:Play/Pause Sel:Skip ÿ3:Stop Time:%d", TimeRemain);
		else
			sprintf(msg1, "ÿ0:FullScreen Dir:Move L1/R1:Zoom Start:Play/Pause Sel:Skip ÿ3:Stop Time:%d", TimeRemain);
		setScrTmp(msg0, msg1);
	} /* end fullscreen */
	drawScr();
} /* end View_Render */

//--------------------------------------------------------------
void View_Input( void ) {

	int i=0;
	u64 OldTime=Timer()+1000;

	while(1){

		if(DiapoStart){
			if(Timer()>=OldTime){
				OldTime=Timer()+1000;
				if(--TimeRemain<=0)
					break;
				View_Render();
			}
		}else{
			OldTime=Timer()+1000;
			TimeRemain=DiapoTime;
		}

		//Pad response section
		waitPadReady(0, 0);
		if(readpad()){
			if(new_pad & PAD_R1){ 
				if( fZoomStep < 3.0f ) { // Zoom In
					fZoomStep += 0.01f;
					DiapoStart=0;
					View_Render();
				} /* end if */
			}else if(new_pad & PAD_L1){ // Zoom Out
				if( fZoomStep > 1.0f ) {
					fZoomStep -= 0.01f;
					DiapoStart=0;
					View_Render();
				} /* end if */
			}else if(new_pad & PAD_RIGHT){ // Move Right
				if ( fOffsetX < 0.95f ) {
					fOffsetX += 0.05f/fZoomStep;
					DiapoStart=0;
					View_Render();
				} /* end if */
			}else if(new_pad & PAD_LEFT){ // Move Left
				if ( fOffsetX > -0.95f ) {
					fOffsetX -= 0.05f/fZoomStep;
					DiapoStart=0;
					View_Render();
				} /* end if */
			}else if(new_pad & PAD_DOWN){ // Move Down
				if ( fOffsetY < 0.95f ) {
					fOffsetY += 0.05f/fZoomStep;
					DiapoStart=0;
					View_Render();
				} /* end if */
			}else if(new_pad & PAD_UP){ // Move Up
				if ( fOffsetY > -0.95f ) {
					fOffsetY -= 0.05f/fZoomStep;
					DiapoStart=0;
					View_Render();
				} /* end if */
			}else if(new_pad & PAD_TRIANGLE){ // Stop Diaporama
				DiapoStop=1;
				break;
			}else if(new_pad & PAD_SELECT){ // Skip Pic
				DiapoSkip=1;
				break;
			}else if(new_pad & PAD_START){ // Play/Pause Diaporama
		    if(fZoomStep!=1.0f){
		    	for ( i = 0; i < 40; ++i ) {
						if (  ( fZoomStep -= 0.05F ) <= 1.0F  ) fZoomStep = 1.0F;
						View_Render();
					} /* end for */
				} /* end if */
				DiapoStart=!DiapoStart;
				WaitTime=Timer();
				while(Timer()<WaitTime+500); // Wait To Ensure On/Off Switch
				TimeRemain=DiapoTime;
				View_Render();
			}else if((swapKeys && new_pad & PAD_CROSS)
			     || (!swapKeys && new_pad & PAD_CIRCLE) ){ // Full Screen
		    if(fZoomStep!=1.0f){
		    	for ( i = 0; i < 40; ++i ) {
						if (  ( fZoomStep -= 0.05F ) <= 1.0F  ) fZoomStep = 1.0F;
						View_Render();
					} /* end for */
				} /* end if */
				FullScreen=!FullScreen;
				WaitTime=Timer();
				while(Timer()<WaitTime+500); // Wait To Ensure On/Off Switch
				View_Render();
			} /* end else */
		}//ends pad response section
	}//ends while
} /* end View_Input */

//--------------------------------------------------------------
void loadPic( char *jpgpath )
{
	int i=0;
	char *name;

	loadSkin( JPG_PIC, jpgpath, 0 );

	name=strrchr(jpgpath, '/');
	strcpy(name, name+1);
	msg0[0]='\0';
	sprintf(msg0, "Jpg Viewer  Picture: %s  Size: %d*%d ", name, (int)picW, (int)picH);
  
	Brightness = 0;
	fZoomStep  = 3.0f;
	fOffsetX   = 0.0f;
	fOffsetY   = 0.0f;
  
	if(testjpg){
		switch ( DiapoTrans ) {
			case OFF  : {
				View_Render();
			} break;
			case ZOOM : {
				Brightness = 100;
		    for ( i = 0; i < 100; ++i ) {
					if (  ( fZoomStep -= 0.02F ) <= 1.0F  ) fZoomStep = 1.0F;
					View_Render();
				} /* end for */
			} break;
			case FOND : {
				fZoomStep = 1.0f;
				for ( i = 0; i < 100; ++i ) {
					++Brightness;
					View_Render();
				} /* end for */
			} break;
			case ZOOM_FOND : {
				for ( i = 0; i < 100; ++i ) {
					if (  ( fZoomStep -= 0.02F ) <= 1.0F  ) fZoomStep = 1.0F;
					++Brightness;
					View_Render();
				} /* end for */
			} break;
		}  /* end switch */

		Brightness = 100;
		fZoomStep  = 1.0f;
		fOffsetX   = 0.0f;
		fOffsetY   = 0.0f;
		TimeRemain = DiapoTime;

		View_Render();
		View_Input();

		switch ( DiapoTrans ) {
			case OFF  : {
				View_Render();
			} break;
			case ZOOM : {
		    for ( i = 0; i < 100; ++i ) {
					if (  ( fZoomStep += 0.02F ) >= 3.0F  ) fZoomStep = 3.0F;
					View_Render();
				} /* end for */
			} break;
			case FOND : {
				for ( i = 0; i < 100; ++i ) {
					--Brightness;
					View_Render();
				} /* end for */
			} break;
			case ZOOM_FOND : {
				for ( i = 0; i < 100; ++i ) {
					if (  ( fZoomStep += 0.02F ) >= 3.0F  ) fZoomStep = 3.0F;
					--Brightness;
					View_Render();
				} /* end for */
			} break;
		}  /* end switch */
	} /* end testjpg */
} /* end loadPic */

//--------------------------------------------------------------
void JpgViewer(void)
{
	char jpgpath[MAX_PATH], path[MAX_PATH],
		cursorEntry[MAX_PATH], ext[8],
		tmp[MAX_PATH], tmp1[MAX_PATH], *p;
	uint64 color;
	FILEINFO files[MAX_ENTRY];
	int thumb_test[MAX_ENTRY];
	int	jpg_browser_cd, jpg_browser_up, jpg_browser_repos, jpg_browser_pushed;
	int thumb_load;
	int	jpg_browser_sel, jpg_browser_nfiles, old_jpg_browser_sel;
	int top=0, test_top=0, rows=0, rows_down=0;
	int x=0, y=0, y0=0, y1=0;
	int thumb_num=0, thumb_top=0, vram_offset=0;
	int jpg_browser_mode=LIST;
	int print_name=0;
	int Len=0;
	int event, post_event=0;
	int i, t=0;
	
	jpg_browser_cd=TRUE;
	jpg_browser_up=FALSE;
	jpg_browser_repos=FALSE;
	jpg_browser_pushed=TRUE;
	thumb_load=TRUE;
	jpg_browser_sel=0;
	jpg_browser_nfiles=0;
	old_jpg_browser_sel=0;
	
	DiapoTime=5;
	DiapoTrans=2;
	DiapoBegin=DiapoStart=DiapoStop=DiapoSkip=FullScreen=0;
	
	for(i=0;i<MAX_ENTRY;++i)
		thumb_test[i]=NOTLOADED;

	strcpy(ext, "jpg");

	mountedParty[0][0]=0;
	mountedParty[1][0]=0;

	path[0]='\0';

	if(jpg_browser_mode==LIST){
		rows = (Menu_end_y-Menu_start_y)/FONT_HEIGHT;
	}else{
		if(TV_mode == TV_mode_NTSC)
			rows=4;
		else if(TV_mode == TV_mode_PAL)
			rows=5;
	}

	loadIcon();

	event = 1;  //event = initial entry
	while(1){

		//Pad response section
		waitPadReady(0, 0);
		if(readpad()){
			if(new_pad){
				jpg_browser_pushed=TRUE;
				print_name=0;
				event |= 2;  //event |= pad command
				tmp[0]=0;
				tmp1[0]=0;
				t=0;
			}
			if(new_pad & PAD_UP)
				jpg_browser_sel--;
			else if(new_pad & PAD_DOWN)
				jpg_browser_sel++;
			else if(new_pad & PAD_LEFT)
				jpg_browser_sel-=rows-1;
			else if(new_pad & PAD_RIGHT){
				rows_down=1;
				old_jpg_browser_sel=jpg_browser_sel+rows-1;
				jpg_browser_sel++;
			}else if(new_pad & PAD_TRIANGLE){
				jpg_browser_up=TRUE;
				thumb_load=TRUE;
			}else if(new_pad & PAD_SQUARE){
				if(jpg_browser_mode==LIST){
					jpg_browser_mode=THUMBNAIL;
					jpg_browser_sel=0;
					thumb_load=TRUE;
				}else
					jpg_browser_mode=LIST;
			}else if(new_pad & PAD_R1){
				if(++DiapoTime>=300) DiapoTime=300;
			}else if(new_pad & PAD_L1){
				if(--DiapoTime<=1) DiapoTime=1;
			}else if(new_pad & PAD_R2){
				if(++DiapoTrans>4) DiapoTrans=1;
			}else if(new_pad & PAD_L2){
				if(--DiapoTrans<1) DiapoTrans=4;
			}else if((swapKeys && new_pad & PAD_CROSS)
			     || (!swapKeys && new_pad & PAD_CIRCLE) ){ //Pushed OK
				if(files[jpg_browser_sel].stats.attrFile & MC_ATTR_SUBDIR){
					//pushed OK for a folder
					thumb_load=TRUE;
					if(!strcmp(files[jpg_browser_sel].name,"..")){
						jpg_browser_up=TRUE;
					}else {
						strcat(path, files[jpg_browser_sel].name);
						strcat(path, "/");
						jpg_browser_cd=TRUE;
					}
				}else{
					//pushed OK for a file
restart:
					sprintf(jpgpath, "%s%s", path, files[jpg_browser_sel].name);

					DiapoBegin=1;

					if(!DiapoStart)
						goto single;
repeat:
					for(i=0;i<jpg_browser_nfiles;++i){
						if(DiapoBegin){
							i=jpg_browser_sel+1;
							DiapoBegin=0;
						}
						sprintf(jpgpath, "%s%s", path, files[i].name);
						loadPic( jpgpath );
						if(DiapoStop)
							goto end;
					} /* end for */
					goto end;
single:
					loadPic( jpgpath );
					if(DiapoSkip || !testjpg){
						if(++jpg_browser_sel>jpg_browser_nfiles) jpg_browser_sel=0;
						DiapoSkip=0;
						goto restart;
					}
end:
					if(DiapoStart && !DiapoStop)
						goto repeat;
					FullScreen=DiapoStart=DiapoStop=0;
					loadSkin(BACKGROUND_PIC, 0, 0);
				} /* end else file */
			} else if(new_pad & PAD_SELECT){
				if(mountedParty[0][0]!=0){
					fileXioUmount("pfs0:");
					mountedParty[0][0]=0;
				} 
				if(mountedParty[1][0]!=0){
					fileXioUmount("pfs1:");
					mountedParty[1][0]=0;
				}
				return;
			} else if(new_pad & PAD_START){
				DiapoStart=1;
				DiapoBegin=0;
				goto repeat;
			}   
		}//ends pad response section
          
		//browser path adjustment section
		if(jpg_browser_up){
			if((p=strrchr(path, '/'))!=NULL)
				*p = 0;
			if((p=strrchr(path, '/'))!=NULL){
				p++;
				strcpy(cursorEntry, p);
				*p = 0;
			}else{
				strcpy(cursorEntry, path);
				path[0] = 0;
			}   
			jpg_browser_cd=TRUE;
			jpg_browser_repos=TRUE;
		}//ends 'if(jpg_browser_up)'
		//----- Process newly entered directory here (incl initial entry)
		if(jpg_browser_cd){
			jpg_browser_nfiles =  setFileList(path, ext, files, JPG_CNF);
			jpg_browser_sel=0;
			top=0;
			if(jpg_browser_repos){
				jpg_browser_repos = FALSE;
				for(i=0; i<jpg_browser_nfiles; i++) {
					if(!strcmp(cursorEntry, files[i].name)) {
						jpg_browser_sel=i;
						top=jpg_browser_sel-3;
						break;
					}
				}
			} //ends if(jpg_browser_repos)
			jpg_browser_cd=FALSE;
			jpg_browser_up=FALSE;
		} //ends if(jpg_browser_cd)
		if(strncmp(path,"cdfs",4) && setting->discControl)
			CDVD_Stop();
		if(top > jpg_browser_nfiles-rows)	top=jpg_browser_nfiles-rows;
		if(top < 0)				top=0;
		if(jpg_browser_sel >= jpg_browser_nfiles)		jpg_browser_sel=jpg_browser_nfiles-1;
		if(jpg_browser_sel < 0)				jpg_browser_sel=0;
		if(jpg_browser_sel >= top+rows)		top=jpg_browser_sel-rows+1;
		if(jpg_browser_sel < top)			top=jpg_browser_sel;
		
		if(thumb_load){
			vram_offset=0;
			for(i=0;i<MAX_ENTRY;++i)
				thumb_test[i]=NOTLOADED;
		}

		t++;

		if(t & 0x0F) event |= 4;  //repetitive timer event

		if(event||post_event){ //NB: We need to update two frame buffers per event
          
			//Display section
			clrScr(setting->color[0]);
          
			if(jpg_browser_mode==LIST){
				x = Menu_start_x;
				y = Menu_start_y;
				rows = (Menu_end_y-Menu_start_y)/FONT_HEIGHT;
				if(rows_down==1){
					jpg_browser_sel=old_jpg_browser_sel;
					rows_down=0;
					old_jpg_browser_sel=0;
				}	/* end if rows_down */
				goto list;
			}else{
				x = Menu_start_x+13;
				if(TV_mode == TV_mode_NTSC){
					y = Menu_start_y+15;
					rows=4;
				}else if(TV_mode == TV_mode_PAL){
					y = Menu_start_y+11;
					rows=5;
				}
			}

			if(test_top!=top){
				if(top>test_top){
down:
					thumb_top=thumb_num=vram_offset=0;
					for(i=0;i<jpg_browser_sel;++i){
						if(i<top && thumb_test[i]==LOADED)
							++thumb_top;
						if(i>=top && thumb_test[i]==LOADED)
							++thumb_num;
					} /* end for */
					if(thumb_test[jpg_browser_sel]==NOTLOADED && !(files[jpg_browser_sel].stats.attrFile & MC_ATTR_SUBDIR)){
						vram_offset=64*32*4*(thumb_top+thumb_num);
						sprintf(jpgpath, "%s%s", path, files[jpg_browser_sel].name);
						loadSkin( THUMB_PIC, jpgpath, 16*16*4*2+vram_offset);
						thumb_test[jpg_browser_sel]=testthumb;
					} /* end if notloaded */
					if(rows_down==1){
						if(++jpg_browser_sel>=old_jpg_browser_sel){
							rows_down=0;
							old_jpg_browser_sel=0;
						}else
							goto down;
					} /* end if rows_down */
				}else{
					thumb_top=0;
					for(i=0;i<top;++i){
						if(thumb_test[i]==LOADED)
							++thumb_top;
					} /* end for */
				} /* end else test_top */
				test_top=top;
			}else{
				if(rows_down==1){
					jpg_browser_sel=old_jpg_browser_sel;
					rows_down=0;
					old_jpg_browser_sel=0;
				}	/* end if rows_down */
			}/* end else test_top!=top */

			thumb_num=0;
			vram_offset=64*32*4*thumb_top;

list:
			for(i=0; i<rows; i++)
			{   
				if(top+i >= jpg_browser_nfiles) break;
				if(top+i == jpg_browser_sel) color = setting->color[2];  //Highlight cursor line
				else			 color = setting->color[3];
				
				if(!strcmp(files[top+i].name,".."))
					strcpy(tmp,"..");
				else
					strcpy(tmp,files[top+i].name);

				if(jpg_browser_mode==LIST){

					if(files[top+i].stats.attrFile & MC_ATTR_SUBDIR)
						strcat(tmp, "/");
					printXY(tmp, x+4, y/2, color, TRUE);
					y += FONT_HEIGHT;

				}else{

					if(files[top+i].stats.attrFile & MC_ATTR_SUBDIR){
						strcat(tmp, "/");
						itoSetTexture(SCREEN_WIDTH*SCREEN_HEIGHT/2*4,
							16, ITO_RGBA32, log(16), log(16));
						itoSetAlphaBlending( ITO_ALPHA_COLOR_SRC,
							ITO_ALPHA_COLOR_DST, ITO_ALPHA_VALUE_SRC, ITO_ALPHA_COLOR_DST, 0x80 );
						itoPrimAlphaBlending(TRUE);
						itoTextureSprite(ITO_RGBA(0, 0, 0, 0x80),
							x+20, (y+10)/2, 0, 0, x+72-20, (y+55-10)/2, 16, 16, 0);
						itoPrimAlphaBlending(FALSE);
						thumb_test[top+i]=BADJPG;
						goto frame;
					}

					if(thumb_load){
						sprintf(jpgpath, "%s%s", path, files[top+i].name);
						loadSkin( THUMB_PIC, jpgpath, 16*16*4*2+vram_offset);
						thumb_test[top+i]=testthumb;
					}

					if(thumb_test[top+i]==LOADED){
						itoSetTexture(SCREEN_WIDTH*SCREEN_HEIGHT/2*4+16*16*4*2+vram_offset,
							64, ITO_RGB24, log(64), log(32));
						itoTextureSprite(ITO_RGBA(0, 0, 0, 0x80),
							x, y/2, 0, 0, x+72, (y+55)/2, 64, 32, 0);
						++thumb_num;
						vram_offset=64*32*4*(thumb_top+thumb_num);
					}else{ // BADJPG
						itoSetTexture(SCREEN_WIDTH*SCREEN_HEIGHT/2*4+16*16*4,
							16, ITO_RGBA32, log(16), log(16));
						itoSetAlphaBlending( ITO_ALPHA_COLOR_SRC,
							ITO_ALPHA_COLOR_DST, ITO_ALPHA_VALUE_SRC, ITO_ALPHA_COLOR_DST, 0x80 );
						itoPrimAlphaBlending(TRUE);
						itoTextureSprite(ITO_RGBA(0, 0, 0, 0x80),
							x+20, (y+10)/2, 0, 0, x+72-20, (y+55-10)/2, 16, 16, 0);
						itoPrimAlphaBlending(FALSE);
					}

frame:
					drawFrame(x, y/2, x+72, (y+55)/2,color);

					Len = printXY(tmp, 0, 0, 0, FALSE);
					if(Len>72 && top+i==jpg_browser_sel){
						if(t%0x10==0){
							strcpy(tmp1,tmp+print_name);
							tmp1[10]='\0';
							if(++print_name>(Len/FONT_WIDTH-10))
								print_name=0;
						}
						Len = printXY(tmp1, 0, 0, 0, FALSE);
						printXY(tmp1, x+72/2-Len/2, (y+58)/2, color, TRUE);
					}else if(Len>72){
						tmp[7]='.'; tmp[8]='.'; tmp[9]='.'; tmp[10]='\0';
						printXY(tmp, x-4, (y+58)/2, color, TRUE);
					}else
						printXY(tmp, x+72/2-Len/2, (y+58)/2, color, TRUE);

					if(TV_mode == TV_mode_NTSC)
						y += 71+15;
					else if(TV_mode == TV_mode_PAL)
						y += 71+11;

				}/* end else THUMBNAIL */

			} //ends for, so all browser rows were fixed above
			thumb_load=FALSE;
			itoSetTexture(0, SCREEN_WIDTH, ITO_RGB24, log(SCREEN_WIDTH), log(SCREEN_HEIGHT/2));
			if(jpg_browser_nfiles > rows) { //if more files than available rows, use scrollbar
				drawFrame(SCREEN_WIDTH-SCREEN_MARGIN-LINE_THICKNESS-15, Frame_start_y/2,
					SCREEN_WIDTH-SCREEN_MARGIN, Frame_end_y/2, setting->color[1]);
				y0=(Menu_end_y-Menu_start_y+8) * ((double)top/jpg_browser_nfiles);
				y1=(Menu_end_y-Menu_start_y+8) * ((double)(top+rows)/jpg_browser_nfiles);
				itoSprite(setting->color[1],
					SCREEN_WIDTH-SCREEN_MARGIN-LINE_THICKNESS-11,
					(y0+Menu_start_y-4)/2,
					SCREEN_WIDTH-SCREEN_MARGIN-LINE_THICKNESS-1,
					(y1+Menu_start_y-4)/2,
					0);
			} //ends clause for scrollbar
			msg0[0]='\0';
			if(jpg_browser_pushed)
				sprintf(msg0, "Jpg Viewer  Path: %s", path);

			//Tooltip section
			msg1[0]='\0';
			if (swapKeys)
				strcpy(msg1, "ÿ1:View");
			else
				strcpy(msg1, "ÿ0:View");
			if(jpg_browser_mode==LIST)
				strcat(msg1, " ÿ3:Up ÿ2:List");
			else
				strcat(msg1, " ÿ3:Up ÿ2:Thumbnail");
			sprintf(tmp, " Sel:Exit Start:Diapo L1/R1:%dsec L2/R2:", DiapoTime);
			strcat(msg1, tmp);
			if(DiapoTrans==OFF)
				strcat(msg1, "Off");
			else if(DiapoTrans==ZOOM)
				strcat(msg1, "Zoom");
			else if(DiapoTrans==FOND)
				strcat(msg1, "Fond");
			else if(DiapoTrans==ZOOM_FOND)
				strcat(msg1, "Zoom+Fond");
			setScrTmp(msg0, msg1);
		}//ends if(event||post_event)
		drawScr();
		post_event = event;
		event = 0;
	}//ends while
} /* end JpgViewer */
//--------------------------------------------------------------
//End of file: jpgviewer.c
//--------------------------------------------------------------
