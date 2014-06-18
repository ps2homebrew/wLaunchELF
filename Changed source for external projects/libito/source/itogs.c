// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
//
// ito - free library for PlayStation 2 by Jules - www.mouthshut.net
//
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------


#include <ito.h>
#include <itogs.h>
#include <itoglobal.h>

void itoSetTEX0();

//-----------------------------------------------------------------
// Active packet in the ITO packet system. 
//-----------------------------------------------------------------

uint64 *itoActivePacket align(128);
uint64 *itoActivePacketStart align(128);

//-----------------------------------------------------------------
// Packet used for video setup and textureloading, etc.
//-----------------------------------------------------------------

uint64 itoPacket[ 20 * 2 ] align(16);

uint8	itoActiveFrameBuffer;
uint8	itoVisibleFrameBuffer;

//-----------------------------------------------------------------
// Global Settings for GS
//-----------------------------------------------------------------

//-----------------------------------------------------------------
//-----------------------------------------------------------------
// ito GS Enviroment Setup
//-----------------------------------------------------------------
//-----------------------------------------------------------------

void itoGsEnvSubmit(itoGsEnv* env)
{
		uint16 width;
		uint8 i;
		
		
		//------------------------------------------------------------
		// Calculate width in VCK units
		// More information on VCK's units avaliabe in dreamtime's
		// tutorials.
		//------------------------------------------------------------
		
		width = env->screen.width >> 6;

		switch(width)
		{
			case 3:		{ ito.screen.mag_x = 13;} break;
			case 4:		{ ito.screen.mag_x = 9; } break;
			case 5:		{ ito.screen.mag_x = 7; } break;
			case 6:		{ ito.screen.mag_x = 6; } break;
			case 7:		{ ito.screen.mag_x = 5; } break;
			case 8:		{ ito.screen.mag_x = 4; } break;
			case 10:	{ ito.screen.mag_x = 3; } break;
			
			//-----------------------------------------------------------------
			// default width to 640 
			//-----------------------------------------------------------------
			default:	{ width = 10; ito.screen.mag_x = 3; } break;
		}	

			width = ((width << 6) - 1) * (ito.screen.mag_x+1);
		
			
		//------------------------------------------------------------
		// GLOBAL VARS
		//------------------------------------------------------------
		
		ito.screen.mag_y = 0; // Do not magnify in vertical direction.

		ito.screen.psm = env->screen.psm;
		ito.screen.x = env->screen.x  * (ito.screen.mag_x+1);
		ito.screen.y = env->screen.y;
		ito.screen.height = env->screen.height;
		ito.screen.width = env->screen.width;

		//-----------------------------------------------------------------
		// offset_x, offset_y calculations taken from duke's lib.
		// Thanks to Duke for explaining the meaning of these values.
		//-----------------------------------------------------------------
		
		//-----------------------------------------------------------------
		// Setup for clipping
		//-----------------------------------------------------------------
		ito.prim.offset_x = ito.screen.offset_x = (4096-env->screen.width)/2;
		ito.prim.offset_y = ito.screen.offset_y = (4096-env->screen.height)/2;
		
		//-----------------------------------------------------------------
		// save buffer 1/2 fbp
		//-----------------------------------------------------------------
		ito.fbp[ITO_FRAMEBUFFER1] = ((env->framebuffer1.y*env->screen.width) + env->framebuffer1.x)*itoGetPixelSize(env->screen.psm);
		ito.fbp[ITO_FRAMEBUFFER2] = ((env->framebuffer2.y*env->screen.width) + env->framebuffer2.x)*itoGetPixelSize(env->screen.psm);
		
		ito.zbuffer.base		= ((env->zbuffer.y*env->screen.width) + env->zbuffer.x)*itoGetPixelSize(env->screen.psm);
		ito.texturebuffer.base	= ito.zbuffer.base + (env->screen.width*env->screen.height*itoGetZPixelSize(env->zbuffer.psm));


		//------------------------------------------------------------
		//------------------------------------------------------------
		
			itoiPutIMR(0xFF00); // // Mask everything, itoGsReset() also does this.
			itoSetGsCrt(env->interlace, env->vmode ,env->ffmode); // Setup interlace, frame and video mode.
			itoSMODE2(env->interlace, env->ffmode, 0);
			itoPMODE(FALSE, TRUE, ALPHA_REG, ITO_FRAME_1, BLEND_RC2, 0xFF); // Use output circuit 2.
			itoDISPFB2(ito.fbp[ITO_FRAMEBUFFER1], env->screen.width, env->screen.psm, 0, 0);
			itoDISPLAY2(ito.screen.x, ito.screen.y, ito.screen.mag_x, ito.screen.mag_y, width, ito.screen.height-1);
		
		// -------------------------------------------------
		// Settings for Buffer 1 & 2
		// -------------------------------------------------
		
		itoSetDmaTDirection( EE2GS );
		itoSetActivePacket( &itoPacket[0] );
		

		itoGifTag(13, TRUE, FALSE, 0, ITO_GIFTAG_TYPE_LIST, 1, ITO_GIFTAG_A_D); // GifTag for Gs Enviroment Settings

		for(i=ITO_FRAMEBUFFER1; i < (ITO_FRAMEBUFFER2+1); i++)
		{
			itoSetActiveFrameBuffer( i );
			itoFRAME(ito.fbp[i], env->screen.width, env->screen.psm, ITO_FRAMEBUFFER_UPDATE);
			itoZBUF(ito.zbuffer.base, env->zbuffer.psm, ITO_ZBUFFER_UPDATE);
			itoXYOFFSET(ito.screen.offset_x << 4, ito.screen.offset_y << 4);
			itoSCISSOR(env->scissor_x1, env->scissor_y1, env->scissor_x2-1, env->scissor_y2-1);
			// no tests, pass everything
			itoTEST(0,0,0,0,0,0, 0, 0);
		}

			itoPRMODECONT(PRMODE_PRIM); // use prim register for prim attributes
			itoCOLCLAMP(TRUE); // Perform ColClamp
			itoDITHER(env->dither);
		
		itoSendActivePacket();
		itoDmaWait();
		itoRestoreDmaDirection();

		// -------------------------------------------------
		// Init
		// -------------------------------------------------
		
		//-----------------------------------------------------------------
		// Misc
		//-----------------------------------------------------------------
			itoResetGsPacket();
			itoInitDoubleFrameBufferMode();
		
		//-----------------------------------------------------------------
		// Prim related
		//-----------------------------------------------------------------
			
//			itoSetTextureCoords(ITO_PRIM_TEXCOORDS_UV);
			itoPrimAntialiasing(FALSE);
			itoPrimFog(FALSE);
			itoPrimShade(ITO_PRIM_SHADE_FLAT);
			itoPrimAlphaBlending(FALSE);
			itoTextureColorComponet(ITO_COLORCOM_RGBA);
			itoTextureLight(ITO_LIGHT_DECAL); // NO LIGHT
		
		// -------------------------------------------------
		// -------------------------------------------------

}


//-----------------------------------------------------------------
//-----------------------------------------------------------------
// Texture Related functions.
//-----------------------------------------------------------------
//-----------------------------------------------------------------

//-----------------------------------------------------------------
// Get Texture Size in bytes
//-----------------------------------------------------------------

uint32 itoGetTextureSize(uint16 w, uint16 h, uint8 psm)
{
	if(psm == ITO_CLUT4)
		return ((w*h)/2);
	else
		return (w*h*itoGetPixelSize(psm));
}

//-----------------------------------------------------------------
// Get Texture Buffer Base
//-----------------------------------------------------------------

uint32 itoGetTextureBufferBase()
{
	return ito.texturebuffer.base;
}

//-----------------------------------------------------------------
// Set Texture BufferBase
//-----------------------------------------------------------------

void itoSetTextureBufferBase(uint32 base)
{
	ito.texturebuffer.base = base;
}



void itoTextureLight(uint8 mode)
{
	ito.texture.tfx = mode;
}

void itoTextureColorComponet(uint8 mode)
{
	ito.texture.tcc = mode;
}


void itoLoadClut(void* src, uint32 dest, uint16 cbw, uint8 psm, uint8 cpsm, uint16 x, uint16 y)
{
	uint16 w = 0, h = 0;
	
	
	switch(cpsm)
	{
		case ITO_CLUT8: { w = 16; h = 16; } break;
		case ITO_CLUT4: { w = 8; h = 2; } break;
		default: break;
	}
	
	if(w)
	{
		itoLoadTexture(src, dest, cbw, psm, x, y, w, h);
	}
}

void itoSetClut( uint8 psm, uint32 base, uint16 offset)
{
	ito.texture.cpsm = psm;
	ito.texture.cbp  = base;
	ito.texture.csa  = offset;
	
	itoSetTEX0();
}


void itoSetTexture(uint32 src, uint32 tbw, uint8 psm, uint16 w, uint16 h)
{
	// This method might change in the future.
	
	ito.texture.src = src;
	ito.texture.tbw = tbw;
	ito.texture.psm = psm;
	ito.texture.w	= w;
	ito.texture.h	= h;
	
	itoSetTEX0();
}

// internal function
void itoSetTEX0()
{
	int i = 0;
	itoGifTag(2, TRUE, FALSE, 0, ITO_GIFTAG_TYPE_LIST, 1, ITO_GIFTAG_A_D);
	
	while(i < 2)
	{
		itoSetActiveFrameBuffer( itoGetActiveFrameBuffer() ^ 1 );
		itoTEX0(ito.texturebuffer.base+ito.texture.src, ito.texture.tbw, ito.texture.psm, ito.texture.w, ito.texture.h, ito.texture.tcc, ito.texture.tfx, ito.texturebuffer.base+ito.texture.cbp, ito.texture.cpsm, 0, ito.texture.csa, 1);
		i++;
	}
	
	itoCheckGsPacket();
}



void itoMoveTexture(	uint8 psm, uint16 w, uint16 h,
						uint32 src,  uint32 s_tbw, uint16 s_x, uint16 s_y, 
						uint32 dest, uint32 d_tbw, uint16 d_x, uint16 d_y
						)
{
		pkt regs[6 * 2];
	
		itoGsFinish();

		itoSetActivePacket( &regs[0] );
		
		itoGifTag(5, TRUE, FALSE, 0, ITO_GIFTAG_TYPE_LIST, 1, ITO_GIFTAG_A_D);
		
		itoBITBLTBUF( src, s_tbw, psm, dest, d_tbw, psm);
		itoTRXPOS(s_x, s_y, d_x, d_y,0);
		itoTRXREG( w, h);
		itoTRXDIR(2); 
		itoTEXFLUSH();
		
		itoSetDmaTDirection(EE2GS);
		itoSendActivePacket();
		itoDmaWait();
		itoRestoreDmaDirection();
		itoInitGsPacket();

}

// -------------------------------------------------
// EE to GS transfer function which uses chained
// DMA mode.
// TODO: Optimize.
// -------------------------------------------------
void itoLoadTexture(void *src, uint32 dest, uint32 tbw, uint8 psm, uint16 x, uint16 y, uint16 w, uint16 h )
{
	
	uint32 packets;
	uint32 packet_rest;
	pkt dmatag[ 22 * 2 ];
	pkt regs[ 62 * 2];
	uint32 i;
	
	// -------------------------------------------------
	// send any prims waiting.
	// -------------------------------------------------
	itoGsFinish();

	// -------------------------------------------------
	// calc number of full packets and size in remaining
	// -------------------------------------------------
		
	packets		= (itoGetTextureSize(w, h, psm) / PACKET_MAX_SIZE);
	packet_rest = (itoGetTextureSize(w, h, psm) % PACKET_MAX_SIZE);

	// -------------------------------------------------
	// setup dma tags for chain transfer
	// -------------------------------------------------
	itoSetActivePacket( &dmatag[0] );
	
	for(i=0; i < packets; i++)
	{	
		itoDmaTag(6, 0, DMA_TAG_ID_REF, 0, regs+(i*6*16), DMA_TAG_SPR_MEM);
		itoDmaTag(PACKET_MAX_SIZE/16, 0, DMA_TAG_ID_REF, 0, src+(i*PACKET_MAX_SIZE), DMA_TAG_SPR_MEM);
	}
	
	i = 0;
	if(packet_rest)
	{
		itoDmaTag(6, 0, DMA_TAG_ID_REF, 0, regs+(packets*6*16), DMA_TAG_SPR_MEM);
		itoDmaTag(packet_rest/16, 0, DMA_TAG_ID_REF, 0, src+(packets*PACKET_MAX_SIZE), DMA_TAG_SPR_MEM);
		i = 1;
	}
	
	// -------------------------------------------------
	// dma tag for texflush
	// -------------------------------------------------
	
	itoDmaTag(2, 0, DMA_TAG_ID_REF, 0, regs+((packets+i)*6*16), DMA_TAG_SPR_MEM);
	// -------------------------------------------------
	// end dma chain transfer
	// -------------------------------------------------
	itoDmaTag(0, 0, DMA_TAG_ID_END, 0, 0, DMA_TAG_SPR_MEM);
	
	// -------------------------------------------------
	// setup GIF packet for sending texture
	// -------------------------------------------------
	itoSetActivePacket( &regs[0] );

	for(i=0; i < packets; i++)
	{
		itoGifTag(4, TRUE, FALSE, 0, ITO_GIFTAG_TYPE_LIST, 1, ITO_GIFTAG_A_D);
		itoBITBLTBUF( 0, 0, 0, ito.texturebuffer.base+dest+(i*PACKET_MAX_SIZE), tbw, psm);
		itoTRXPOS(0,0, x, y,0);
		itoTRXREG( w, h);
		itoTRXDIR(0); 
		itoGifTag( PACKET_MAX_SIZE / 16, TRUE, FALSE, 0, ITO_GIFTAG_TYPE_IMAGE, 1, 0);
	}
	
	i=0;
	if(packet_rest)
	{
		itoGifTag(4, TRUE, FALSE, 0, ITO_GIFTAG_TYPE_LIST, 1, ITO_GIFTAG_A_D);
		itoBITBLTBUF( 0, 0, 0, ito.texturebuffer.base+dest+(packets*PACKET_MAX_SIZE), tbw, psm);
		itoTRXPOS(0,0, x, y,0);
		itoTRXREG( w, h);
		itoTRXDIR(0); 
		itoGifTag( packet_rest / 16, TRUE, FALSE, 0, ITO_GIFTAG_TYPE_IMAGE, 1, 0);
	
	}	
	
	// -------------------------------------------------
	// TEXFLUSH
	// -------------------------------------------------
	itoGifTag(1, TRUE, FALSE, 0, ITO_GIFTAG_TYPE_LIST, 1, ITO_GIFTAG_A_D);
	itoTEXFLUSH();
	
	itoiFlushCache(0);
	itoDma2Wait();
	itoDma2TagSend( &dmatag[0] );
	
	itoInitGsPacket();

}

uint8 itoGetPixelSize(uint8 mode)
{
	switch(mode) {
	
	case ITO_RGBA32: return 4; break;
	case ITO_RGB24:  return 3; break;
	case ITO_RGBA16: return 2; break;
	case ITO_CLUT8:	 return 1; break;

	default: return 0;
	}


}


//-----------------------------------------------------------------
//-----------------------------------------------------------------
// Prim Settings
//-----------------------------------------------------------------
//-----------------------------------------------------------------

//-----------------------------------------------------------------
// Use STQ or UV coords
//-----------------------------------------------------------------
void itoSetTextureCoords(uint8 mode)
{
	ito.prim.texture_coords = mode;
}
//-----------------------------------------------------------------
// for use parameter use FALSE or TRUE
//-----------------------------------------------------------------

void itoPrimAlphaBlending(uint8 use)
{
	ito.prim.alpha = use;
}

void itoPrimFog(uint8 use)
{
	ito.prim.fog = use;
}

void itoPrimAntialiasing(uint8 use)
{
	ito.prim.antialiasing = use;
}

void itoPrimShade(uint8 mode)
{
	ito.prim.shade = mode;
}


//-----------------------------------------------------------------
//-----------------------------------------------------------------
// Privileged Registers
//-----------------------------------------------------------------
//-----------------------------------------------------------------

//-----------------------------------------------------------------
// Output Circuit 2 is only used, only because Duke/NpM uses it 
// in his '3 Stars' demo, which source I used to get the very basic 
// GS functionality working.
//-----------------------------------------------------------------

//-----------------------------------------------------------------
// SMODE2
//-----------------------------------------------------------------
void itoSMODE2(uint8 interlace, uint8 ffmode, uint8 vesa)
{
	GS_SMODE2 =  ((uint64)vesa << 2) | ((uint64)ffmode << 1) | interlace; 
}
//-----------------------------------------------------------------
// PMODE
//-----------------------------------------------------------------
void itoPMODE(uint8 rc1, uint8 rc2, uint8 alpha_select, uint8 alpha_output, uint8 alpha_blend, uint8 alpha)
{
	GS_PMODE =  ((uint64)(alpha << 8) | ((uint64)alpha_blend << 7) | ((uint64)alpha_output << 6) | ((uint64)alpha_select << 5) | ((uint64)0x1 << 2) | ((uint64)rc2) << 1) | rc1;
}

//-----------------------------------------------------------------
// DISPFB2
//-----------------------------------------------------------------
void itoDISPFB2(uint32 fbp, uint32 fbw, uint8 psm, uint16 dbx, uint16 dby)
{
	GS_DISPFB2  = ((uint64)dby << 43) | ((uint64)dbx << 32) |((uint64)psm << 15) | ((uint64)(fbw/64) << 9) | (fbp/(2048*4)) ;
}

//-----------------------------------------------------------------
// DISPLAY2
//-----------------------------------------------------------------
void itoDISPLAY2(uint16 dx, uint16 dy, uint8 mag_x, uint8 mag_y, uint16 dw, uint16 dh)
{
	GS_DISPLAY2 = (((uint64)dh) << 44) | (((uint64)dw) << 32) | (((uint64)mag_y) << 27) | (((uint64)mag_x) << 23) | (((uint64)dy) << 12) | ((uint64)dx);
}

//-----------------------------------------------------------------
// BUSDIR
//-----------------------------------------------------------------
void itoBUSDIR( uint8 dir )
{
	GS_BUSDIR = dir;
}


//-----------------------------------------------------------------
//-----------------------------------------------------------------
// GS General Purpose Registers
//-----------------------------------------------------------------
//-----------------------------------------------------------------

//-----------------------------------------------------------------
// Custom function for texture mapping both STQ/UV
// set by itoSetTextureCoords(..)
//-----------------------------------------------------------------

void itoSTUV(uint32 tx, uint32 ty)
{
	if( ito.prim.texture_coords == ITO_PRIM_TEXCOORDS_STQ )	itoST(tx, ty);
	
	if( ito.prim.texture_coords == ITO_PRIM_TEXCOORDS_UV )	itoUV((uint16)(tx << 4), (uint16)(ty << 4));
}

//-----------------------------------------------------------------
// itoGsSetActivePacket is used to define what packet to use.
// itoSetActiveFrameBuffer is used to define which of the two 
// contexts (framebuffers) to use.
//-----------------------------------------------------------------


//-----------------------------------------------------------------
// FOG
//-----------------------------------------------------------------
void itoFOG(uint8 f)
{
	itoActivePacket[0] = (uint64)f << 56;
	itoActivePacket[1] = GS_REG_FOG+itoActiveFrameBuffer;
	itoActivePacket+=2;
}

//-----------------------------------------------------------------
// FOGCOL
//-----------------------------------------------------------------
void itoFOGCOL(uint8 fcr, uint8 fcg, uint8 fcb)
{
	itoActivePacket[0] = (uint64)fcb << 16 | (uint64)fcg << 8 | fcr;
	itoActivePacket[1] = GS_REG_FOGCOL+itoActiveFrameBuffer;
	itoActivePacket+=2;
}

//-----------------------------------------------------------------
// ALPHA
//-----------------------------------------------------------------

void itoALPHA(uint8 a, uint8 b, uint8 c, uint8 d, uint8 fix)
{
	itoActivePacket[0] = ((uint64)fix << 32) | ((uint64)d << 6) | ((uint64)c << 4) | ((uint64)b << 2)| ((uint64)a);
	itoActivePacket[1] = GS_REG_ALPHA_1+itoActiveFrameBuffer ;
	itoActivePacket+=2;
}

//-----------------------------------------------------------------
// BITBLTBUF
//-----------------------------------------------------------------
void itoBITBLTBUF(uint32 sbp, uint16 sbw, uint16 spsm, uint32 dbp, uint16 dbw, uint16 dpsm)
{
	itoActivePacket[0] = sbp >> 8;
	itoActivePacket[0] |= (uint64)(sbw >> 6) << 16;
	itoActivePacket[0] |= (uint64)spsm << 24;
	itoActivePacket[0] |= (uint64)(dbp >> 8) << 32 ;
	itoActivePacket[0] |= (uint64)(dbw >> 6) << 48;// << 48;
	itoActivePacket[0] |= (uint64)dpsm << 56;
	itoActivePacket[1] = GS_REG_BITBLTBUF;
	itoActivePacket+=2;
}

//-----------------------------------------------------------------
// COLCLAMP
//-----------------------------------------------------------------
void itoCOLCLAMP( uint64 colclamp)
{
	itoActivePacket[0] = colclamp;
	itoActivePacket[1] = GS_REG_COLCLAMP;
	itoActivePacket+=2;
}

//-----------------------------------------------------------------
// DITHER
//-----------------------------------------------------------------
void itoDITHER( uint64 dither)
{
	itoActivePacket[0] = dither;
	itoActivePacket[1] = GS_REG_DTHE;
	itoActivePacket+=2;
}


//-----------------------------------------------------------------
// FRAME
//-----------------------------------------------------------------
void itoFINISH()
{
	itoActivePacket[0] = 1; // any value will do
	itoActivePacket[1] = GS_REG_FINISH;
	itoActivePacket+=2;
}

//-----------------------------------------------------------------
// FRAME
//-----------------------------------------------------------------
void itoFRAME(uint32 fb_base, uint32 fb_width, uint8 psm, uint32 update)
{
	itoActivePacket[0] = (((uint64)update) << 32) | (((uint64)psm) << 24) | ((uint32)((fb_width >> 6) << 16)) | (fb_base >> 13);  
	itoActivePacket[1] = GS_REG_FRAME_1+itoActiveFrameBuffer ;
	itoActivePacket+=2;
}

//-----------------------------------------------------------------
// PRIM
//-----------------------------------------------------------------
void itoPRIM(uint8 prim, uint8 iip, uint8 tme, uint8 fge, uint8 abe, uint8 aai, uint8 fst, uint8 ctxt, uint8 fix)
{
	itoActivePacket[0] = ((uint64)fix << 10) | ((uint64)ctxt << 9) | ((uint64)fst << 8) | ((uint64)aai << 7) | ((uint64)abe << 6) | ((uint64)fge << 5) | ((uint64)tme << 4) | ((uint64)iip << 3) | prim;
	itoActivePacket[1] = GS_REG_PRIM;
	itoActivePacket+=2;
}
//-----------------------------------------------------------------
// PRMODECONT
//-----------------------------------------------------------------
void itoPRMODECONT( uint64 prmodecont)
{
	itoActivePacket[0] = prmodecont;
	itoActivePacket[1] = GS_REG_PRMODECONT;
	itoActivePacket+=2;
}
//-----------------------------------------------------------------
// RGBAQ
//-----------------------------------------------------------------
void itoRGBAQ(uint64 rgbaq)
{
	itoActivePacket[0] = rgbaq;
	itoActivePacket[1] = GS_REG_RGBAQ;
	itoActivePacket+=2;

}

//-----------------------------------------------------------------
// SCISSOR
//-----------------------------------------------------------------
void itoSCISSOR(uint16 scissor_x1, uint16 scissor_y1, uint16 scissor_x2, uint16 scissor_y2)
{
	itoActivePacket[0] = (((uint64)(scissor_y2)) << 48) | (((uint64)(scissor_y1)) << 32) | (((uint64)(scissor_x2)) << 16) | scissor_x1;
	itoActivePacket[1] = GS_REG_SCISSOR_1+itoActiveFrameBuffer;
	itoActivePacket+=2;
}

//-----------------------------------------------------------------
// ST
//-----------------------------------------------------------------
void itoST(uint32 s, uint32 t)
{
	//itoActivePacket[0] = 0x3F8000003F800000;
	itoActivePacket[0] = ((uint64)t << 32) | ((uint64)s);
	itoActivePacket[1] = GS_REG_ST;
	itoActivePacket+=2;

}

//-----------------------------------------------------------------
// TEST
//-----------------------------------------------------------------
void itoTEST( uint8 ate, uint8 atst, uint8 aref, uint8 afail, uint8 date, uint8 datm, uint8 zte, uint8 ztst)
{
	itoActivePacket[0] = ate;
	itoActivePacket[0] |= (uint64)atst << 1;
	itoActivePacket[0] |= (uint64)aref << 4;
	itoActivePacket[0] |= (uint64)afail << 12;
	itoActivePacket[0] |= (uint64)date << 14;
	itoActivePacket[0] |= (uint64)datm << 15;
	itoActivePacket[0] |= (uint64)zte << 16;
	itoActivePacket[0] |= (uint64)ztst << 17;
	// GLOBAL USED
	ito.regs.test = itoActivePacket[0];
	itoActivePacket[1] = GS_REG_TEST_1+itoActiveFrameBuffer;
	itoActivePacket+=2;
}

//-----------------------------------------------------------------
// TEX0
//-----------------------------------------------------------------
void itoTEX0(uint32 tbp0, uint16 tbw, uint8 psm,uint16 tw,uint16 th, uint8 tcc, uint8 tfx, uint32 cbp, uint8 cpsm,uint8 csm, uint8 csa, uint8 cld)
{
	itoActivePacket[0] =   tbp0 >> 8 ;
	itoActivePacket[0] |= (uint64)(tbw >> 6) << 14;
	itoActivePacket[0] |= (uint64)psm << 20;
	itoActivePacket[0] |= (uint64)tw << 26;
	itoActivePacket[0] |= (uint64)th << 30;
	itoActivePacket[0] |= (uint64)tcc << 34;
	itoActivePacket[0] |= (uint64)tfx << 35;
	itoActivePacket[0] |= (uint64)(cbp / 256) << 37;
	itoActivePacket[0] |= (uint64)cpsm << 51;
	itoActivePacket[0] |= (uint64)csm << 55;
	itoActivePacket[0] |= (uint64)(csa/16) << 56;
	itoActivePacket[0] |= (uint64)cld << 61;
	itoActivePacket[1] = GS_REG_TEX0_1+itoActiveFrameBuffer;
	
	itoActivePacket+=2;
}
//-----------------------------------------------------------------
// TEXA
//-----------------------------------------------------------------
void itoTEXA(uint8 ta0, uint8 aem, uint8 ta1)
{
	itoActivePacket[0] = ((uint64)ta1 << 32) | ((uint64)aem << 15) | ta0;
	itoActivePacket[1] = GS_REG_TEXA;
	itoActivePacket+=2;
}

//-----------------------------------------------------------------
// TEXFLUSH
//-----------------------------------------------------------------
void itoTEXFLUSH()
{
	itoActivePacket[0] = 1; // Any value will do.
	itoActivePacket[1] = GS_REG_TEXFLUSH;
	itoActivePacket+=2;
}

//-----------------------------------------------------------------
// TRXDIR
//-----------------------------------------------------------------
void itoTRXDIR(uint8 dir)
{
	itoActivePacket[0] = dir;
	itoActivePacket[1] = GS_REG_TRXDIR;
	itoActivePacket+=2;
}

//-----------------------------------------------------------------
// TRXPOS
//-----------------------------------------------------------------
void itoTRXPOS(uint16 src_x,uint16 src_y,uint16 dst_x,uint16 dst_y, uint8 dir)
{
	itoActivePacket[0] = src_x;
	itoActivePacket[0] |= ((uint64)src_y) << 16;
	itoActivePacket[0] |= ((uint64)dst_x) << 32;
	itoActivePacket[0] |= ((uint64)dst_y) << 48;
	itoActivePacket[0] |= ((uint64)dir) << 59;
	itoActivePacket[1] = GS_REG_TRXPOS;
	itoActivePacket+=2;
}

//-----------------------------------------------------------------
// TRXREG
//-----------------------------------------------------------------
void itoTRXREG(uint16 width, uint16 height)
{
	itoActivePacket[0] = width;
	itoActivePacket[0] |= ((uint64)height) << 32;
	itoActivePacket[1] = GS_REG_TRXREG;
	itoActivePacket+=2;
}

//-----------------------------------------------------------------
// UV
//-----------------------------------------------------------------
void itoUV(uint16 u, uint16 v)
{
	itoActivePacket[0] = ((uint64)v << 16) | ((uint64)u);
	itoActivePacket[1] = GS_REG_UV;
	itoActivePacket+=2;
}

//-----------------------------------------------------------------
// XYOFFSET
//-----------------------------------------------------------------
void itoXYOFFSET(uint32 offset_x, uint32 offset_y)
{
	itoActivePacket[0] = ((uint64)offset_y << 32) | offset_x;
	itoActivePacket[1] = GS_REG_XYOFFSET_1+itoActiveFrameBuffer;
	itoActivePacket+=2;
}

//-----------------------------------------------------------------
// XYZ2
//-----------------------------------------------------------------

uint64 itoXYZ2(uint16 x, uint16 y, uint32 z)
{	
	itoActivePacket[0] = ((uint64) z << 32) | ((uint64)y << 16) | x;
	itoActivePacket[1] = GS_REG_XYZ2;
	itoActivePacket+=2;
	return itoActivePacket[-2];	
}

//-----------------------------------------------------------------
// ZBUF
//-----------------------------------------------------------------
void itoZBUF(uint32 zb_base, uint8 psm, uint8 update)
{
	itoActivePacket[0] = ((uint64)update << 32) | ((uint64)psm << 24)| ((uint64)(zb_base >> 13));
	itoActivePacket[1]  = GS_REG_ZBUF_1+itoActiveFrameBuffer;
	itoActivePacket+=2;
}

//-----------------------------------------------------------------
// MISC
//-----------------------------------------------------------------

void itoVSync()
{
	GS_CSR &= 8; // generate
	while(!(GS_CSR & 8)); // wait until its generated
}

void itoGsReset()
{
	GS_CSR = GS_CSR_RESET;
}


//-----------------------------------------------------------------
//-----------------------------------------------------------------
// GS interface (GIF)
//-----------------------------------------------------------------
//-----------------------------------------------------------------

void itoGifTag(uint16 nloop, uint8 eop, uint8 pre, uint16 prim, uint8 flag,uint8 nreg, uint64 regs)
{
	itoActivePacket[0] = ((uint64)nreg << 60) | ((uint64)flag << 58) | ((uint64)prim << 47) | ((uint64)pre << 46) | ((uint64)eop << 15) | nloop;
	itoActivePacket[1] = regs;
	itoActivePacket+=2;
}



//-----------------------------------------------------------------
// Double buffer
//-----------------------------------------------------------------

void itoInitDoubleFrameBufferMode()
{
	itoSetActiveFrameBuffer(ITO_FRAMEBUFFER1);
	itoSetVisibleFrameBuffer(ITO_FRAMEBUFFER2);
}

void itoSwitchFrameBuffers()
{
	itoSetVisibleFrameBuffer(itoGetVisibleFrameBuffer() ^ 1);
	itoSetActiveFrameBuffer(itoGetActiveFrameBuffer() ^ 1);
}

//-----------------------------------------------------------------
// Framebuffers
//-----------------------------------------------------------------

uint32 itoGetFrameBufferBase(uint8 buffer)
{
	return  ito.fbp[buffer];
}

void itoSetFrameBufferBase(uint8 buffer, uint32 base)
{
	ito.fbp[buffer] = base;
}

void itoFrameBufferUpdate(uint8 update)
{

	update ^= 1;

	itoGifTag(2, TRUE, FALSE, 0, ITO_GIFTAG_TYPE_LIST, 1, ITO_GIFTAG_TYPE_LIST);

	itoSetActiveFrameBuffer( itoGetActiveFrameBuffer() ^ 1 );
	itoFRAME(ito.fbp[ itoGetActiveFrameBuffer() ], ito.screen.width, ito.screen.psm, update);
	
	itoSetActiveFrameBuffer( itoGetActiveFrameBuffer() ^ 1 );
	itoFRAME(ito.fbp[ itoGetActiveFrameBuffer() ], ito.screen.width, ito.screen.psm, update);
	
	itoCheckGsPacket();

}

//-----------------------------------------------------------------
// Active Frame Buffer
//-----------------------------------------------------------------

void itoSetActiveFrameBuffer(uint8 buffer)
{
	itoActiveFrameBuffer = buffer;
}

uint8 itoGetActiveFrameBuffer()
{
	return itoActiveFrameBuffer;
}

//-----------------------------------------------------------------
// Visible Frame Buffer
//-----------------------------------------------------------------
void itoSetVisibleFrameBuffer(uint8 buffer)
{
	itoVisibleFrameBuffer = buffer;
	// this does the actual switch 
	itoDISPFB2(ito.fbp[buffer], ito.screen.width, ito.screen.psm, 0, 0);
}

uint8 itoGetVisibleFrameBuffer()
{
	return itoVisibleFrameBuffer;
}

//-----------------------------------------------------------------
// ZBuffer
//-----------------------------------------------------------------

uint8 itoGetZPixelSize(uint8 mode)
{
	switch(mode) {
	
	case ITO_ZBUF32:	return 4; break;
	case ITO_ZBUF24:	return 3; break;
	case ITO_ZBUF16:	return 2; break;
	case ITO_ZBUF16S:	return 2; break;

	default: return 0;
	}


}

void itoZBufferUpdate(uint8 update)
{

	update ^= 1;

	itoGifTag(2, TRUE, FALSE, 0, ITO_GIFTAG_TYPE_LIST, 1, ITO_GIFTAG_A_D);

	itoSetActiveFrameBuffer( itoGetActiveFrameBuffer() ^ 1 );
	itoZBUF(ito.zbuffer.base, ito.zbuffer.psm, update);
	
	itoSetActiveFrameBuffer( itoGetActiveFrameBuffer() ^ 1 );
	itoZBUF(ito.zbuffer.base, ito.zbuffer.psm, update);
	
	itoCheckGsPacket();
}

uint32 itoGetZBufferBase()
{
	return ito.zbuffer.base;
}

void itoSetZBufferBase(uint32 base)
{
	ito.zbuffer.base = base;	
}

//-----------------------------------------------------------------
// Tests
//-----------------------------------------------------------------

void itoZBufferTest(uint8 use, uint8 method)
{
	
	ito.zbuffer.test_method = method;
	ito.zbuffer.test		= use;

	ito.regs.test &= 0x7FFF;
	ito.regs.test |= (uint64)use << 16;
	ito.regs.test |= (uint64)method << 17;
	
	itoGifTag(2, TRUE, FALSE, 0, ITO_GIFTAG_TYPE_LIST, 1, ITO_GIFTAG_A_D);
	itoActivePacket[0] = ito.regs.test;
	itoActivePacket[1] = GS_REG_TEST_1;
	itoActivePacket[2] = ito.regs.test;
	itoActivePacket[3] = GS_REG_TEST_2;
	itoActivePacket+=4;


	itoCheckGsPacket();
}

void itoAlphaValue(uint8 use, uint8 a0, uint8 a1, uint8 tob)
{
	itoTextureColorComponet(use);
	
	if(use)
	{
		itoGifTag(1, TRUE, FALSE, 0, ITO_GIFTAG_TYPE_LIST, 1, ITO_GIFTAG_A_D);
		itoTEXA(a0, tob, a1);
		itoCheckGsPacket();
	}
}

void itoAlphaTest(uint8 use, uint8 method, uint8 value, uint8 failed)
{
	//-----------------------------------------------------------------
	// Only Set alpha part of test register
	//-----------------------------------------------------------------

	ito.regs.test &= 0x38000;
	ito.regs.test |= (use & 0x1);
	ito.regs.test |= (uint64)(method & 0x7) << 1;
	ito.regs.test |= (uint64)(value & 0xFF) << 4;
	ito.regs.test |= (uint64)(failed & 0x3) << 12;
	
	//-----------------------------------------------------------------
	// setup packet
	//-----------------------------------------------------------------
	itoGifTag(2, TRUE, FALSE, 0, ITO_GIFTAG_TYPE_LIST, 1, ITO_GIFTAG_A_D);
	itoActivePacket[0] = ito.regs.test;
	itoActivePacket[1] = GS_REG_TEST_1;
	itoActivePacket[2] = ito.regs.test;
	itoActivePacket[3] = GS_REG_TEST_2;

	itoActivePacket+=4;

	itoCheckGsPacket();
}

//-----------------------------------------------------------------
// Screen Related
//-----------------------------------------------------------------

void itoSetOffsetXY(uint16 x, uint16 y)
{
	
	itoGifTag(2, TRUE, FALSE, 0, ITO_GIFTAG_TYPE_LIST, 1, ITO_GIFTAG_A_D);
	
	itoSetActiveFrameBuffer( itoGetActiveFrameBuffer() ^ 1 );
	itoXYOFFSET(x << 4, y << 4);
	
	itoSetActiveFrameBuffer( itoGetActiveFrameBuffer() ^ 1 );
	itoXYOFFSET(x << 4, y << 4);
	
	itoCheckGsPacket();

}

void itoSetPrimXY(uint16 x, uint16 y)
{
	ito.prim.offset_x = x;
	ito.prim.offset_y = y;
}

void itoSetScreenPos(uint16 x, uint16 y)
{
	x = x * (ito.screen.mag_x+1);
	//-----------------------------------------------------------------
	// using global vars
	//-----------------------------------------------------------------
	itoDISPLAY2(x, y, ito.screen.mag_x, ito.screen.mag_y, (ito.screen.width-1)*(ito.screen.mag_x+1), ito.screen.height -1 );
}

void itoSetFogValue(uint8 fcr, uint8 fcg, uint8 fcb, uint8 f)
{
	itoGifTag(4, TRUE, FALSE, 0,ITO_GIFTAG_TYPE_LIST, 1, ITO_GIFTAG_A_D);
	
	itoSetActiveFrameBuffer( itoGetActiveFrameBuffer() ^ 1 );
	itoFOGCOL(fcr, fcg, fcb);
	itoFOG(f);
	
	itoSetActiveFrameBuffer( itoGetActiveFrameBuffer() ^ 1 );
	itoFOGCOL(fcr, fcg, fcb);
	itoFOG(f);

	itoCheckGsPacket();
}

void itoSetAlphaBlending(uint8 a, uint8 b, uint8 c, uint8 d, uint8 fix)
{
	itoGifTag(2, TRUE, FALSE, 0,ITO_GIFTAG_TYPE_LIST, 1, ITO_GIFTAG_A_D);
	
	itoSetActiveFrameBuffer( itoGetActiveFrameBuffer() ^ 1 );
	itoALPHA(a, b, c, d, fix);
	
	itoSetActiveFrameBuffer( itoGetActiveFrameBuffer() ^ 1 );
	itoALPHA(a, b, c, d, fix);

	itoCheckGsPacket();
}

void itoSetScissor(uint16 x1, uint16 y1, uint16 x2, uint16 y2)
{
	itoGifTag(2, TRUE, FALSE, 0,ITO_GIFTAG_TYPE_LIST, 1, ITO_GIFTAG_A_D);
	
	itoSetActiveFrameBuffer( itoGetActiveFrameBuffer() ^ 1 );
	itoSCISSOR(x1, y1, x2-1, y2-1);
	
	itoSetActiveFrameBuffer( itoGetActiveFrameBuffer() ^ 1 );
	itoSCISSOR(x1, y1, x2-1, y2-1);

	itoCheckGsPacket();
}


void itoSetBgColor(uint32 color)
{
	GS_BGCOLOR = color;
}
