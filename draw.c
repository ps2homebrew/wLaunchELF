//--------------------------------------------------------------
//File name:   draw.c
//--------------------------------------------------------------
#include "launchelf.h"
#include "font5200.c"

itoGsEnv	screen_env;
int				testskin;
int				testsetskin;
int				SCREEN_WIDTH	= 640;
int				SCREEN_HEIGHT = 448;
int				SCREEN_X			= 158;
int				SCREEN_Y			= 26;
//dlanor: values shown above are defaults for NTSC mode

char LastMessage[MAX_TEXT_LINE+2];

int Menu_start_x   = SCREEN_MARGIN + LINE_THICKNESS + FONT_WIDTH;
int Menu_title_y   = SCREEN_MARGIN;
int Menu_message_y = SCREEN_MARGIN + FONT_HEIGHT;
int Frame_start_y  = SCREEN_MARGIN + 2*FONT_HEIGHT + 2;  //First line of menu frame
int Menu_start_y   = SCREEN_MARGIN + 2*FONT_HEIGHT + LINE_THICKNESS + 6;
//dlanor: Menu_start_y is the 1st pixel line that may be used for main content of a menu
//dlanor: values below are only calculated when a rez is activated
int Menu_end_y;     //Normal menu display should not use pixels at this line or beyond
int Frame_end_y;    //first line of frame bottom
int Menu_tooltip_y; //Menus may also use this row for tooltips


//The font file ELISA100.FNT is needed to display MC save titles in japanese
//and the arrays defined here are needed to find correct data in that file
const uint16 font404[] = {
	0xA2AF, 11,
	0xA2C2, 8,
	0xA2D1, 11,
	0xA2EB, 7,
	0xA2FA, 4,
	0xA3A1, 15,
	0xA3BA, 7,
	0xA3DB, 6,
	0xA3FB, 4,
	0xA4F4, 11,
	0xA5F7, 8,
	0xA6B9, 8,
	0xA6D9, 38,
	0xA7C2, 15,
	0xA7F2, 13,
	0xA8C1, 720,
	0xCFD4, 43,
	0xF4A5, 1030,
	0,0
};

// ASCII‚ÆSJIS‚Ì•ÏŠ·—p”z—ñ
const unsigned char sjis_lookup_81[256] = {
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,  // 0x00
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,  // 0x10
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,  // 0x20
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,  // 0x30
  ' ', ',', '.', ',', '.', 0xFF,':', ';', '?', '!', 0xFF,0xFF,'´', '`', 0xFF,'^',   // 0x40
  0xFF,'_', 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,'0', 0xFF,'-', '-', 0xFF,0xFF,  // 0x50
  0xFF,0xFF,0xFF,0xFF,0xFF,'\'','\'','"', '"', '(', ')', 0xFF,0xFF,'[', ']', '{',   // 0x60
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,'+', '-', 0xFF,'*', 0xFF,  // 0x70
  '/', '=', 0xFF,'<', '>', 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,'°', 0xFF,0xFF,'°', 0xFF,  // 0x80
  '$', 0xFF,0xFF,'%', '#', '&', '*', '@', 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,  // 0x90
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,  // 0xA0
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,  // 0xB0
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,'&', '|', '!', 0xFF,0xFF,0xFF,0xFF,0xFF,  // 0xC0
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,  // 0xD0
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,  // 0xE0
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,  // 0xF0
};
const unsigned char sjis_lookup_82[256] = {
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,  // 0x00
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,  // 0x10
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,  // 0x20
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,  // 0x30
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,'0',   // 0x40
  '1', '2', '3', '4', '5', '6', '7', '8', '9', 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,  // 0x50
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',   // 0x60
  'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,  // 0x70
  0xFF,'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',   // 0x80
  'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', 0xFF,0xFF,0xFF,0xFF,0xFF,  // 0x90
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,  // 0xA0
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,  // 0xB0
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,  // 0xC0
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,  // 0xD0
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,  // 0xE0
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,  // 0xF0
};

int log( int Value ){
	int r = 0;
	Value--;
	while( Value > 0 )
	{
		Value = Value >> 1;
		r++;
	}
	return r;
}
//--------------------------------------------------------------
int *CreateCoeffInt( int nLen, int nNewLen, int bShrink ) {

 int nSum    = 0;
 int nSum2   = 0;
 int *pRes   = (int*) malloc( 2 * nLen * sizeof(int) );
 int *pCoeff = pRes;
 int nNorm   = (bShrink) ? (nNewLen << 12) / nLen : 0x1000;
 int nDenom  = (bShrink) ? nLen : nNewLen;
 int i;

 memset( pRes, 0, 2 * nLen * sizeof(int) );

 for( i = 0; i < nLen; i++, pCoeff += 2 ) {

  nSum2 = nSum + nNewLen;

  if(nSum2 > nLen) {

   *pCoeff = ((nLen - nSum) << 12) / nDenom;
   pCoeff[1] = ((nSum2 - nLen) << 12) / nDenom;
   nSum2 -= nLen;

  } else {

   *pCoeff = nNorm;

   if(nSum2 == nLen) {
    pCoeff[1] = -1;
    nSum2 = 0;

   } /* end if */

  } /* end else */

  nSum = nSum2;

 } /* end for */

 return pRes;

} /* CreateCoeffInt */

//--------------------------------------------------------------
int ShrinkData( u8 *pInBuff, u16 wWidth, u16 wHeight, u8 *pOutBuff, u16 wNewWidth, u16 wNewHeight ) {

 u8 *pLine    = pInBuff, *pPix;
 u8 *pOutLine = pOutBuff;
 u32 dwInLn   = ( 3 * wWidth + 3 ) & ~3;
 u32 dwOutLn  = ( 3 * wNewWidth + 3 ) & ~3;

 int  x, y, i, ii;
 int  bCrossRow, bCrossCol;
 int *pRowCoeff = CreateCoeffInt( wWidth, wNewWidth, 1 );
 int *pColCoeff = CreateCoeffInt( wHeight, wNewHeight, 1 );

 int *pXCoeff, *pYCoeff = pColCoeff;
 u32  dwBuffLn = 3 * wNewWidth * sizeof( u32 );
 u32 *pdwBuff = ( u32* ) malloc( 6 * wNewWidth * sizeof( u32 ) );
 u32 *pdwCurrLn = pdwBuff;  
 u32 *pdwNextLn = pdwBuff + 3 * wNewWidth;
 u32 *pdwCurrPix;
 u32  dwTmp, *pdwNextPix;

 memset( pdwBuff, 0, 2 * dwBuffLn );

 y = 0;
 
 while( y < wNewHeight ) {

  pPix = pLine;
  pLine += dwInLn;

  pdwCurrPix = pdwCurrLn;
  pdwNextPix = pdwNextLn;

  x = 0;
  pXCoeff = pRowCoeff;
  bCrossRow = pYCoeff[ 1 ] > 0;
  
  while( x < wNewWidth ) {
  
   dwTmp = *pXCoeff * *pYCoeff;
   for ( i = 0; i < 3; i++ )
    pdwCurrPix[ i ] += dwTmp * pPix[i];
   
   bCrossCol = pXCoeff[ 1 ] > 0;
   
   if ( bCrossCol ) {

    dwTmp = pXCoeff[ 1 ] * *pYCoeff;
    for ( i = 0, ii = 3; i < 3; i++, ii++ )
     pdwCurrPix[ ii ] += dwTmp * pPix[ i ];

   } /* end if */

   if ( bCrossRow ) {

    dwTmp = *pXCoeff * pYCoeff[ 1 ];
    for( i = 0; i < 3; i++ )
     pdwNextPix[ i ] += dwTmp * pPix[ i ];
    
    if ( bCrossCol ) {

     dwTmp = pXCoeff[ 1 ] * pYCoeff[ 1 ];
     for ( i = 0, ii = 3; i < 3; i++, ii++ )
      pdwNextPix[ ii ] += dwTmp * pPix[ i ];

    } /* end if */

   } /* end if */

   if ( pXCoeff[ 1 ] ) {

    x++;
    pdwCurrPix += 3;
    pdwNextPix += 3;

   } /* end if */
   
   pXCoeff += 2;
   pPix += 3;

  } /* end while */
  
  if ( pYCoeff[ 1 ] ) {

   // set result line
   pdwCurrPix = pdwCurrLn;
   pPix = pOutLine;
   
   for ( i = 3 * wNewWidth; i > 0; i--, pdwCurrPix++, pPix++ )
    *pPix = ((u8*)pdwCurrPix)[3];

   // prepare line buffers
   pdwCurrPix = pdwNextLn;
   pdwNextLn = pdwCurrLn;
   pdwCurrLn = pdwCurrPix;
   
   memset( pdwNextLn, 0, dwBuffLn );
   
   y++;
   pOutLine += dwOutLn;

  } /* end if */
  
  pYCoeff += 2;

 } /* end while */

 free( pRowCoeff );
 free( pColCoeff );
 free( pdwBuff );

 return 1;

} /* end ShrinkData */

//--------------------------------------------------------------
int EnlargeData( u8 *pInBuff, u16 wWidth, u16 wHeight, u8 *pOutBuff, u16 wNewWidth, u16 wNewHeight ) {

 u8 *pLine = pInBuff,
    *pPix  = pLine,
    *pPixOld,
    *pUpPix,
    *pUpPixOld;
 u8 *pOutLine = pOutBuff, *pOutPix;
 u32 dwInLn   = ( 3 * wWidth + 3 ) & ~3;
 u32 dwOutLn  = ( 3 * wNewWidth + 3 ) & ~3;

 int x, y, i;
 int bCrossRow, bCrossCol;
 
 int *pRowCoeff = CreateCoeffInt( wNewWidth, wWidth, 0 );
 int *pColCoeff = CreateCoeffInt( wNewHeight, wHeight, 0 );
 int *pXCoeff, *pYCoeff = pColCoeff;
 
 u32 dwTmp, dwPtTmp[ 3 ];
 
 y = 0;
 
 while( y < wHeight ) {
 
  bCrossRow = pYCoeff[ 1 ] > 0;
  x         = 0;
  pXCoeff   = pRowCoeff;
  pOutPix   = pOutLine;
  pOutLine += dwOutLn;
  pUpPix    = pLine;
  
  if ( pYCoeff[ 1 ] ) {

   y++;
   pLine += dwInLn;
   pPix = pLine;

  } /* end if */
  
  while( x < wWidth ) {

   bCrossCol = pXCoeff[ 1 ] > 0;
   pUpPixOld = pUpPix;
   pPixOld  = pPix;
   
   if( pXCoeff[ 1 ] ) {

    x++;
    pUpPix += 3;
    pPix += 3;

   } /* end if */
   
   dwTmp = *pXCoeff * *pYCoeff;
   
   for ( i = 0; i < 3; i++ )
    dwPtTmp[ i ] = dwTmp * pUpPixOld[ i ];

   if ( bCrossCol ) {

    dwTmp = pXCoeff[ 1 ] * *pYCoeff;
    for ( i = 0; i < 3; i++ )
     dwPtTmp[ i ] += dwTmp * pUpPix[ i ];

   } /* end if */

   if ( bCrossRow ) {

    dwTmp = *pXCoeff * pYCoeff[ 1 ];
    for ( i = 0; i < 3; i++ )
     dwPtTmp[ i ] += dwTmp * pPixOld[ i ];
    
    if ( bCrossCol ) {

     dwTmp = pXCoeff[ 1 ] * pYCoeff[ 1 ];
     for(i = 0; i < 3; i++)
      dwPtTmp[ i ] += dwTmp * pPix[ i ];

    } /* end if */

   } /* end if */
   
   for ( i = 0; i < 3; i++, pOutPix++ )
    *pOutPix = (  ( u8* )( dwPtTmp + i )  )[ 3 ];
   
   pXCoeff += 2;

  } /* end while */
  
  pYCoeff += 2;

 } /* end while */
 
 free( pRowCoeff );
 free( pColCoeff );

 return 1;

} /* end EnlargeData */

//--------------------------------------------------------------
int ScaleBitmap( u8* pInBuff, u16 wWidth, u16 wHeight, u8** pOutBuff, u16 wNewWidth, u16 wNewHeight ) {

 int lRet;

 // check for valid size
 if( wWidth > wNewWidth && wHeight < wNewHeight ) return 0;

 if( wHeight > wNewHeight && wWidth < wNewWidth ) return 0;

 // allocate memory
 *pOutBuff = ( u8* ) memalign(   128, (  ( 3 * wNewWidth + 3 ) & ~3  ) * wNewHeight   );

 if( !*pOutBuff )return 0;

 if( wWidth >= wNewWidth && wHeight >= wNewHeight )
  lRet = ShrinkData( pInBuff, wWidth, wHeight, *pOutBuff, wNewWidth, wNewHeight );
 else
  lRet = EnlargeData( pInBuff, wWidth, wHeight, *pOutBuff, wNewWidth, wNewHeight );

 return lRet;
 
} /* end ScaleBitmap */

//--------------------------------------------------------------
// Init Screen
void initScr(void)
{
	itoSprite( ITO_RGBA( 0x00, 0x00, 0x00, 0 ), 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0 );
	itoGsFinish();
	itoSwitchFrameBuffers();
	itoSprite( ITO_RGBA( 0x00, 0x00, 0x00, 0 ), 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0 );
	itoGsFinish();
	itoSwitchFrameBuffers();
}

//--------------------------------------------------------------
void setScrTmp(const char *msg0, const char *msg1)
{
	int x, y;
	
	x = SCREEN_MARGIN;
	y = Menu_title_y;
	printXY(setting->Menu_Title, x, y/2, setting->color[3], TRUE);
	printXY(" ÿ4 LaunchELF v3.72 ÿ4",
		SCREEN_WIDTH-SCREEN_MARGIN-FONT_WIDTH*22, y/2, setting->color[1], TRUE);
	
	strncpy(LastMessage, msg0, MAX_TEXT_LINE);
	LastMessage[MAX_TEXT_LINE] = '\0';
	printXY(msg0, x, Menu_message_y/2, setting->color[2], TRUE);

	if(setting->Menu_Frame)
		drawFrame(SCREEN_MARGIN, Frame_start_y/2,
			SCREEN_WIDTH-SCREEN_MARGIN, Frame_end_y/2, setting->color[1]);
	
	printXY(msg1, x, Menu_tooltip_y/2, setting->color[2], TRUE);
}
//--------------------------------------------------------------
void drawSprite( uint64 color, int x1, int y1, int x2, int y2 ){
	if ( testskin == 1 ) {
		setBrightness(setting->Brightness);
		itoTextureSprite(ITO_RGBAQ( 0x80, 0x80, 0x80, 0xFF, 0 ), x1, y1, x1, y1, x2, y2, x2, y2,0);
		setBrightness(50);
	}else{
		itoSprite(color, x1, y1, x2, y2, 0);
	}
}
//--------------------------------------------------------------
void drawPopSprite( uint64 color, int x1, int y1, int x2, int y2 ){
	if ( testskin == 1 && !setting->Popup_Opaque) {
		setBrightness(setting->Brightness);
		itoTextureSprite(ITO_RGBAQ( 0x80, 0x80, 0x80, 0xFF, 0 ), x1, y1, x1, y1, x2, y2, x2, y2,0);
		setBrightness(50);
	}else{
		itoSprite(color, x1, y1, x2, y2, 0);
	}
}
//--------------------------------------------------------------
void drawMsg(const char *msg)
{
	strncpy(LastMessage, msg, MAX_TEXT_LINE);
	LastMessage[MAX_TEXT_LINE] = '\0';
	drawSprite(setting->color[0], 0, (Menu_message_y)/2,
		SCREEN_WIDTH, (Frame_start_y)/2);
	printXY(msg, SCREEN_MARGIN, Menu_message_y/2, setting->color[2], TRUE);
	drawScr();
}
//--------------------------------------------------------------
void drawLastMsg(void)
{
	drawSprite(setting->color[0], 0, (Menu_message_y)/2,
		SCREEN_WIDTH, (Frame_start_y)/2);
	printXY(LastMessage, SCREEN_MARGIN, Menu_message_y/2, setting->color[2], TRUE);
	drawScr();
}
//--------------------------------------------------------------
void setupito(int ito_vmode)
{
	itoInit();

	// screen resolution
	screen_env.screen.width		= SCREEN_WIDTH;
	screen_env.screen.height	= SCREEN_HEIGHT;
	screen_env.screen.psm			= ITO_RGBA32;

	// These setting work best with my tv, experiment for youself
	screen_env.screen.x				= setting->screen_x; 
	screen_env.screen.y				= setting->screen_y;
	
	screen_env.framebuffer1.x	= 0;
	screen_env.framebuffer1.y	= SCREEN_HEIGHT;
	
	screen_env.framebuffer2.x	= 0;
	screen_env.framebuffer2.y	= SCREEN_HEIGHT*2;

	// zbuffer
	screen_env.zbuffer.x			= 0;
	screen_env.zbuffer.y			= SCREEN_HEIGHT*2;
	screen_env.zbuffer.psm		= ITO_ZBUF32;
	
	// scissor 
	screen_env.scissor_x1			= 0;
	screen_env.scissor_y1			= 0;
	screen_env.scissor_x2			= SCREEN_WIDTH;
	screen_env.scissor_y2			= SCREEN_HEIGHT;
	
	// misc
	screen_env.dither					= TRUE;
	screen_env.interlace			= setting->interlace;
	screen_env.ffmode					= ITO_FRAME;
	screen_env.vmode					= ito_vmode;

	itoGsEnvSubmit(&screen_env);
	initScr();
	itoSetBgColor(GS_border_colour);
}

//--------------------------------------------------------------
void updateScreenMode(void)
{
	screen_env.screen.x = setting->screen_x;
	screen_env.screen.y = setting->screen_y;
	screen_env.interlace = setting->interlace;
	itoGsReset();
	itoGsEnvSubmit(&screen_env);
}
//--------------------------------------------------------------
void loadSkin(int Picture)
{
	if( Picture == BACKGROUND_PIC )testskin = 0;
		else testsetskin = 0;
	char skinpath[MAX_PATH];
	strcpy(skinpath, "\0");
	if(!strncmp(setting->skin, "mass", 4)){
		loadUsbModules();
		strcpy(skinpath, "mass:");
		strcat(skinpath, setting->skin+6);
	}else if(!strncmp(setting->skin, "hdd0:/", 6)){
		char party[MAX_PATH];
		char *p;

		loadHddModules();
		sprintf(party, "hdd0:%s", setting->skin+6);
		p = strchr(party, '/');
		sprintf(skinpath, "pfs0:%s", p);
		*p = 0;
		fileXioMount("pfs0:", party, FIO_MT_RDONLY);
	}else if(!strncmp(setting->skin, "cdfs", 4)){
		loadCdModules();
		strcpy(skinpath, setting->skin);
		CDVD_FlushCache();
		CDVD_DiskReady(0);
	}else{
		strcpy(skinpath,setting->skin);
	}
	FILE*	File = fopen( skinpath, "r" );
 
	if( File != NULL ) {

		jpgData* Jpg;
		u8*      Buf;
		u8*      ImgData;
		u8*      ResData;
		long     Size;

		fseek ( File, 0, SEEK_END ); 
		Size = ftell ( File ); 
		fseek ( File, 0, SEEK_SET ); 

		if( Size ){
			if( ( Buf = malloc( Size ) ) > 0 ){
				fread( Buf, 1, Size, File );
				if( ( Jpg = jpgOpenRAW ( Buf, Size, JPG_WIDTH_FIX ) ) > 0 ){
					if( (ImgData = malloc( Jpg->width * Jpg->height * ( Jpg->bpp / 8 ) ) ) > 0){
						if( ( jpgReadImage( Jpg, ImgData ) ) != -1 ){
						 	if( Picture == BACKGROUND_PIC ){
						 		if( ( ScaleBitmap ( ImgData, Jpg->width, Jpg->height, &ResData, SCREEN_WIDTH, SCREEN_HEIGHT/2 ) ) != 0 ){
						 			itoLoadTexture ( ResData,
						 			 0,
						 			 SCREEN_WIDTH, ITO_RGB24,
						 			 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT/2 );
									itoGsFinish();
									testskin = 1;
								} /* end if */
						 	} else {
						 		if( ( ScaleBitmap ( ImgData, Jpg->width, Jpg->height, &ResData, SCREEN_WIDTH/2, SCREEN_HEIGHT/4 ) ) != 0 ){
						 			itoLoadTexture ( ResData,
						 			 SCREEN_WIDTH*SCREEN_HEIGHT/2*4,
						 			 SCREEN_WIDTH/2, ITO_RGB24,
						 			 0, 0, SCREEN_WIDTH/2, SCREEN_HEIGHT/4 );
									itoGsFinish();
									testsetskin = 1;
								} /* end if */
						 	} /* end else */
							jpgClose( Jpg );//This really should be moved, but jpg funcs may object
							free(ResData); //This really should be moved, but jpg funcs may object
						} /* end if((jpgReadImage(...)) != -1) */
					free(ImgData);
					} /* end if( (ImgData = malloc(...)) > 0 ) */
				} /* end if( (Jpg=jpgOpenRAW(...)) > 0 ) */
			free(Buf);
			} /* end if( ( Buf = malloc( Size ) ) > 0 ) */
		} /* end if( Size ) */
		fclose( File );
	} /* end if( File != NULL ) */
	if(!strncmp(setting->skin, "cdfs", 4)) CDVD_Stop();
	if(!strncmp(setting->skin, "hdd0:/", 6)) fileXioUmount("pfs0:");
}

//--------------------------------------------------------------
// Set Skin Brightness
void setBrightness(int Brightness)
{
	int		fogStat	= FALSE;
	int		fogColor	= 0;
	int		fogCoef	= 255;
	float adjustBright = 128.0F;
 
	if ( Brightness == 50 ) adjustBright = 128.0F;
		else	adjustBright = (((256.0F/100.0F)*Brightness) + (32.0F*((50.0F-Brightness)/50)));

	if ( adjustBright <=   32.0F ) adjustBright = 32.0F;

	if ( adjustBright == 128.0F ) {
 		fogStat  = FALSE;
		fogColor = 0;
		fogCoef  = 255;
	} /* end if */

	if ( adjustBright >= 224.0F ) adjustBright = 224.0F;
 
	if ( adjustBright >= 32.0F && adjustBright < 128.0F ) {
 		fogStat  = TRUE;
		fogColor = 0;
		if( ( fogCoef  = (int)adjustBright * 2 ) <= 0 ) fogCoef = 0;
	} /* end if */

	if ( adjustBright > 128.0F && adjustBright <= 224.0F ) {
 		fogStat  = TRUE;
		fogColor = 255;
		if( ( fogCoef  = 320 - ( (int)adjustBright - 96 ) * 2 ) >= 256 ) fogCoef = 255;
	} /* end if */

	itoPrimFog( fogStat );
	itoSetFogValue( fogColor, fogColor, fogColor, fogCoef );
}

//--------------------------------------------------------------
void clrScr(uint64 color)
{
	if ( testskin == 1 ) {
		setBrightness(setting->Brightness);
		itoSprite(ITO_RGBA( 0x00, 0x00, 0x00, 0 ), 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0);
		itoSetTexture(0, SCREEN_WIDTH, ITO_RGB24, log(SCREEN_WIDTH), log(SCREEN_HEIGHT/2));
		itoTextureSprite(ITO_RGBAQ(0x80, 0x80, 0x80, 0xFF, 0), 0, 0, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT/2, SCREEN_WIDTH, SCREEN_HEIGHT/2, 0);
		setBrightness(50);
	} else {
		itoSprite(color, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0);
	} /* end else */
}

//--------------------------------------------------------------
void drawScr(void)
{
	itoGsFinish();
	itoVSync();
	itoSwitchFrameBuffers();
}

//--------------------------------------------------------------
void drawFrame(int x1, int y1, int x2, int y2, uint64 color)
{
	itoLine(color, x1, y1, 0, color, x2, y1, 0);
	
	itoLine(color, x2, y1, 0, color, x2, y2, 0);
	itoLine(color, x2-1, y1, 0, color, x2-1, y2, 0);
	
	itoLine(color, x2, y2, 0, color, x1, y2, 0);
	
	itoLine(color, x1, y2, 0, color, x1, y1, 0);
	itoLine(color, x1+1, y2, 0, color, x1+1, y1, 0);
}

//--------------------------------------------------------------
// draw a char using the system font (8x8)
void drawChar(unsigned char c, int x, int y, uint64 colour)
{
	unsigned int i, j, ix;
	unsigned char cc;

	if(c < 0x20)       ix=(0x5F-0x20)*8;
	else if(c >= 0xAA) ix=(0x5F-0x20)*8;
	else               ix=(c-0x20)*8;
	for(i=0; i<8; i++)
	{
		cc = font5200[ix++];
		for(j=0; j<8; j++)
		{
			if(cc & 0x80) itoPoint(colour, x+j, y+i, 0);
			cc = cc << 1;
		}
	}
}
//------------------------------
//endfunc drawChar
//--------------------------------------------------------------
// draw a char using the ELISA font (8x8)
void drawChar2(int32 n, int x, int y, uint64 colour)
{
	unsigned int i, j;
	uint8 b;
	
	for(i=0; i<8; i++)
	{
		b = elisaFnt[n+i];
		for(j=0; j<8; j++)
		{
			if(b & 0x80) {
				itoPoint(colour, x+j, y+i, 0);
			}
			b = b << 1;
		}
	}
}
//------------------------------
//endfunc drawChar2
//--------------------------------------------------------------
// draw a string of characters, without shift-JIS support
int printXY(const unsigned char *s, int x, int y, uint64 colour, int draw)
{
	unsigned char c1, c2;
	int i;
	
	i=0;
	while((c1=s[i++])!=0) {
		if(c1 != 0xFF) { // Normal character
			if(draw) drawChar(c1, x, y, colour);
			x += 8;
			if(x > SCREEN_WIDTH-SCREEN_MARGIN-FONT_WIDTH)
				break;
			continue;
		}  //End if for normal character
		// Here we got a sequence starting with 0xFF ('ÿ')
		if((c2=s[i++])==0)
			break;
		if((c2 < '0') || (c2 > '4'))
			continue;
		c1=(c2-'0')*2+0xA0;
		if(draw) {
			//expand sequence ÿ0=Circle  ÿ1=Cross  ÿ2=Square  ÿ3=Triangle  ÿ4=FilledBox
			drawChar(c1, x, y, colour);
			x += 8;
			if(x > SCREEN_WIDTH-SCREEN_MARGIN-FONT_WIDTH)
				break;
			drawChar(c1+1, x, y, colour);
			x += 8;
			if(x > SCREEN_WIDTH-SCREEN_MARGIN-FONT_WIDTH)
				break;
		}
	}  // ends while(1)
	return x;
}
//------------------------------
//endfunc printXY
//--------------------------------------------------------------
// draw a string of characters, with shift-JIS support (only for gamesave titles)
int printXY_sjis(const unsigned char *s, int x, int y, uint64 colour, int draw)
{
	int32 n;
	unsigned char ascii;
	uint16 code;
	int i, j, tmp;
	
	i=0;
	while(s[i]){
		if((s[i] & 0x80) && s[i+1]) { //we have top bit and some more char ?
			// SJIS
			code = s[i++];
			code = (code<<8) + s[i++];
			
			switch(code){
			// Circle == "›"
			case 0x819B:
				if(draw){
					drawChar(160, x, y, colour);
					drawChar(161, x+8, y, colour);
				}
				x+=16;
				break;
			// Cross == "~"
			case 0x817E:
				if(draw){
					drawChar(162, x, y, colour);
					drawChar(163, x+8, y, colour);
				}
				x+=16;
				break;
			// Square == " "
			case 0x81A0:
				if(draw){
					drawChar(164, x, y, colour);
					drawChar(165, x+8, y, colour);
				}
				x+=16;
				break;
			// Triangle == "¢"
			case 0x81A2:
				if(draw){
					drawChar(166, x, y, colour);
					drawChar(167, x+8, y, colour);
				}
				x+=16;
				break;
			// FilledBox == "¡"
			case 0x81A1:
				if(draw){
					drawChar(168, x, y, colour);
					drawChar(169, x+8, y, colour);
				}
				x+=16;
				break;
			default:
				if(elisaFnt!=NULL){ // elisa font is available ?
					tmp=y;
					if(code<=0x829A) tmp++;
					// SJIS‚©‚çEUC‚É•ÏŠ·
					if(code >= 0xE000) code-=0x4000;
					code = ((((code>>8)&0xFF)-0x81)<<9) + (code&0x00FF);
					if((code & 0x00FF) >= 0x80) code--;
					if((code & 0x00FF) >= 0x9E) code+=0x62;
					else code-=0x40;
					code += 0x2121 + 0x8080;
					
					// EUC‚©‚çŒb—œ¹ƒtƒHƒ“ƒg‚Ì”Ô†‚ð¶¬
					n = (((code>>8)&0xFF)-0xA1)*(0xFF-0xA1)
						+ (code&0xFF)-0xA1;
					j=0;
					while(font404[j]) {
						if(code >= font404[j]) {
							if(code <= font404[j]+font404[j+1]-1) {
								n = -1;
								break;
							} else {
								n-=font404[j+1];
							}
						}
						j+=2;
					}
					n*=8;
					
					if(n>=0 && n<=55008) {
						if(draw) drawChar2(n, x, tmp, colour);
						x+=9;
					}else{
						if(draw) drawChar('_', x, y, colour);
						x+=8;
					}
				}else{ //elisa font is not available
					ascii=0xFF;
					if(code>>8==0x81)
						ascii = sjis_lookup_81[code & 0x00FF];
					else if(code>>8==0x82)
						ascii = sjis_lookup_82[code & 0x00FF];
					if(ascii!=0xFF){
						if(draw) drawChar(ascii, x, y, colour);
					}else{
						if(draw) drawChar('_', x, y, colour);
					}
					x+=8;
				}
				break;
			}
		}else{ //First char does not have top bit set or no following char
			if(draw) drawChar(s[i], x, y, colour);
			i++;
			x += 8;
		}
		if(x > SCREEN_WIDTH-SCREEN_MARGIN-FONT_WIDTH){
			//x=16; y=y+8;
			return x;
		}
	}
	return x;
}
//------------------------------
//endfunc printXY_sjis
//--------------------------------------------------------------
//End of file: draw.c
//--------------------------------------------------------------
