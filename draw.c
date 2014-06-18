//--------------------------------------------------------------
//File name:   draw.c
//--------------------------------------------------------------
#include "launchelf.h"
#include "font5200.c"

itoGsEnv screen_env;
int        testskin;
int        testsetskin;

// ELISA100.FNTに存在しない文字
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

// ASCIIとSJISの変換用配列
const unsigned char sjis_lookup_81[256] = {
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,  // 0x00
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,  // 0x10
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,  // 0x20
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,  // 0x30
  ' ', ',', '.', ',', '.', 0xFF,':', ';', '?', '!', 0xFF,0xFF,'ｴ', '`', 0xFF,'^',   // 0x40
  0xFF,'_', 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,'0', 0xFF,'-', '-', 0xFF,0xFF,  // 0x50
  0xFF,0xFF,0xFF,0xFF,0xFF,'\'','\'','"', '"', '(', ')', 0xFF,0xFF,'[', ']', '{',   // 0x60
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,'+', '-', 0xFF,'*', 0xFF,  // 0x70
  '/', '=', 0xFF,'<', '>', 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,'ｰ', 0xFF,0xFF,'ｰ', 0xFF,  // 0x80
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
	y = SCREEN_MARGIN;
	printXY(setting->Menu_Title, x, y/2, setting->color[3], TRUE);
	printXY(" ■ LaunchELF v3.57 ■",
		SCREEN_WIDTH-SCREEN_MARGIN-FONT_WIDTH*22, y/2, setting->color[1], TRUE);
	y += FONT_HEIGHT+4;
	
	printXY(msg0, x, y/2, setting->color[2], TRUE);
	y += FONT_HEIGHT;
	
	if(setting->Menu_Frame)
		drawFrame(SCREEN_MARGIN, y/2,
			SCREEN_WIDTH-SCREEN_MARGIN-1,
			(SCREEN_HEIGHT-SCREEN_MARGIN-FONT_HEIGHT-2)/2,
			setting->color[1]);

	x = SCREEN_MARGIN;
	y = SCREEN_HEIGHT-SCREEN_MARGIN-FONT_HEIGHT;
	printXY(msg1, x, y/2, setting->color[2], TRUE);
}
//--------------------------------------------------------------
void drawSprite( uint64 color, int x1, int y1, int x2, int y2 ){
	if ( testskin == 1 ) {
		itoTextureSprite(ITO_RGBAQ( 0x80, 0x80, 0x80, 0xFF, 0 ),x1, y1,x1, y1,x2, y2,x2, y2,0);
	}else{
		itoSprite(color,x1, y1,x2, y2,0);
	}
}
//--------------------------------------------------------------
void drawMsg(const char *msg)
{
	drawSprite(setting->color[0],
		0, (SCREEN_MARGIN+FONT_HEIGHT+4)/2,
		SCREEN_WIDTH, (SCREEN_MARGIN+FONT_HEIGHT+4+FONT_HEIGHT)/2);
	printXY(msg, SCREEN_MARGIN, (SCREEN_MARGIN+FONT_HEIGHT+4)/2,
		setting->color[2], TRUE);
	drawScr();
}
//--------------------------------------------------------------
void setupito(void)
{
	itoInit();

	// screen resolution
	screen_env.screen.width		= 512;
	screen_env.screen.height	= 480;
	screen_env.screen.psm		= ITO_RGBA32;

	// These setting work best with my tv, experiment for youself
	screen_env.screen.x			= setting->screen_x; 
	screen_env.screen.y			= setting->screen_y;
	
	screen_env.framebuffer1.x	= 0;
	screen_env.framebuffer1.y	= 0;
	
	screen_env.framebuffer2.x	= 0;
	screen_env.framebuffer2.y	= 480;

	// zbuffer
	screen_env.zbuffer.x		= 0;
	screen_env.zbuffer.y		= 480*2;
	screen_env.zbuffer.psm		= ITO_ZBUF32;
	
	// scissor 
	screen_env.scissor_x1		= 0;
	screen_env.scissor_y1		= 0;
	screen_env.scissor_x2		= 512;
	screen_env.scissor_y2		= 480;
	
	// misc
	screen_env.dither			= TRUE;
	screen_env.interlace		= setting->interlace;
	screen_env.ffmode			= ITO_FRAME;
	screen_env.vmode			= ITO_VMODE_AUTO;
	
	itoGsEnvSubmit(&screen_env);
	initScr();
	itoSetBgColor(setting->color[0]);
}

//--------------------------------------------------------------
void loadSkin(int skinNum)
{
	if( skinNum == 0 )testskin = 0;
		else testsetskin = 0;
	char skinpath[MAX_PATH];
	strcpy(skinpath, "\0");
	if(!strncmp(setting->skin, "mass", 4)){
		loadUsbModules();
		strcpy(skinpath, "mass:");
		strcat(skinpath, setting->skin+6);
	}else if(!strncmp(setting->skin, "hdd0:/", 6)){
		loadHddModules();
		char party[40];
		char *p;
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
				fclose( File );
				if( ( Jpg = jpgOpenRAW ( Buf, Size, JPG_WIDTH_FIX ) ) > 0 ){
					if( ( ImgData = malloc (  Jpg -> width * Jpg -> height * ( Jpg -> bpp / 8 ) ) ) > 0 ){
						if( ( jpgReadImage( Jpg, ImgData ) ) != -1 ){
						 	if( skinNum == 0 ){
						 		if( ( ScaleBitmap ( ImgData, Jpg -> width, Jpg -> height, &ResData, SCREEN_WIDTH, 240 ) ) != 0 ){
						 			itoLoadTexture ( ResData, 0, SCREEN_WIDTH, ITO_RGB24, 0, 0, SCREEN_WIDTH, 240 );
									itoGsFinish();
									testskin = 1;
								} /* end if */
						 	} else {
						 		if( ( ScaleBitmap ( ImgData, Jpg -> width, Jpg -> height, &ResData, 256, 120 ) ) != 0 ){
						 			itoLoadTexture ( ResData, SCREEN_WIDTH*256*4, 256, ITO_RGB24, 0, 0, 256, 120 );
									itoGsFinish();
									testsetskin = 1;
								} /* end if */
						 	} /* end else */
							jpgClose( Jpg );
							free(ResData);
						} /* end if */
					} /* end if */
					free(ImgData);
				} /* end if */
			} /* end if */
			free(Buf);
		} /* end if */
	} /* end if */
	if(!strncmp(setting->skin, "cdfs", 4)) CDVD_Stop();
	if(!strncmp(setting->skin, "hdd0:/", 6)) fileXioUmount("pfs0:");
}

//--------------------------------------------------------------
void clrScr(uint64 color)
{
	if ( testskin == 1 ) {
		itoSprite(ITO_RGBA( 0x00, 0x00, 0x00, 0 ), 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0);
		itoSetTexture(0, 512, ITO_RGB24, log(SCREEN_WIDTH), log(240));
		itoTextureSprite(ITO_RGBAQ(0x80, 0x80, 0x80, 0xFF, 0), 0, 0, 0, 0, SCREEN_WIDTH, 240, SCREEN_WIDTH, 240, 0);
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
	unsigned int i, j;
	unsigned char cc;

	for(i=0; i<8; i++)
	{
		cc = font5200[(c-32)*8+i];
		for(j=0; j<8; j++)
		{
			if(cc & 0x80) itoPoint(colour, x+j, y+i, 0);
			cc = cc << 1;
		}
	}
}

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

//--------------------------------------------------------------
// draw a string of characters
int printXY(const unsigned char *s, int x, int y, uint64 colour, int draw)
{
	int32 n;
	unsigned char ascii;
	uint16 code;
	int i, j, tmp;
	
	i=0;
	while(s[i]){
		if(s[i] & 0x80) {
			// SJISコードの生成
			code = s[i++];
			code = (code<<8) + s[i++];
			
			switch(code){
			// ○
			case 0x819B:
				if(draw){
					drawChar(160, x, y, colour);
					drawChar(161, x+8, y, colour);
				}
				x+=16;
				break;
			// ×
			case 0x817E:
				if(draw){
					drawChar(162, x, y, colour);
					drawChar(163, x+8, y, colour);
				}
				x+=16;
				break;
			// □
			case 0x81A0:
				if(draw){
					drawChar(164, x, y, colour);
					drawChar(165, x+8, y, colour);
				}
				x+=16;
				break;
			// △
			case 0x81A2:
				if(draw){
					drawChar(166, x, y, colour);
					drawChar(167, x+8, y, colour);
				}
				x+=16;
				break;
			// ■
			case 0x81A1:
				if(draw){
					drawChar(168, x, y, colour);
					drawChar(169, x+8, y, colour);
				}
				x+=16;
				break;
			default:
				if(elisaFnt!=NULL){
					tmp=y;
					if(code<=0x829A) tmp++;
					// SJISからEUCに変換
					if(code >= 0xE000) code-=0x4000;
					code = ((((code>>8)&0xFF)-0x81)<<9) + (code&0x00FF);
					if((code & 0x00FF) >= 0x80) code--;
					if((code & 0x00FF) >= 0x9E) code+=0x62;
					else code-=0x40;
					code += 0x2121 + 0x8080;
					
					// EUCから恵梨沙フォントの番号を生成
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
				}else{
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
		}else{
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
//--------------------------------------------------------------
//End of file: draw.c
//--------------------------------------------------------------
