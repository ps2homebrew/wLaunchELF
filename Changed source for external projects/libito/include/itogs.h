#ifndef _ITOGS_H
#define _ITOGS_H

#ifdef __cplusplus 
	extern "C" {
#endif /* __cplusplus */

#include <itotypes.h>

//-----------------------------------------------------------------
// GENERAL PURPOSE REGISTERS
//-----------------------------------------------------------------

#define GS_REG_PRMODECONT	0x1A
#define GS_REG_COLCLAMP		0x46
#define GS_REG_RGBAQ		0x01
#define GS_REG_DTHE			0x45
#define GS_REG_BITBLTBUF	0x50
#define GS_REG_TRXPOS		0x51
#define GS_REG_TRXREG		0x52
#define GS_REG_TRXDIR		0x53
#define GS_REG_PRIM			0x00
#define GS_REG_UV			0x03
#define GS_REG_XYZ2			0x05
#define GS_REG_TEXFLUSH		0x3F
#define GS_REG_ST			0x02
#define GS_REG_FINISH		0x61
#define GS_REG_TEXA			0x3B
#define GS_REG_FOG			0x0A
#define GS_REG_FOGCOL		0x3D

//-----------------------------------------------------------------
// PRIM TYPES
//-----------------------------------------------------------------
#define ITO_PRIM_POINT			0x0
#define ITO_PRIM_LINE			0x1
#define ITO_PRIM_LINE_STRIP		0x2
#define ITO_PRIM_TRIANGLE		0x3
#define ITO_PRIM_TRIANGLE_STRIP	0x4
#define ITO_PRIM_TRIANGLE_FAN	0x5
#define ITO_PRIM_SPRITE			0x6

#define GS_REG_ALPHA_1			0x42
#define GS_REG_TEST_1			0x47
#define GS_REG_FRAME_1			0x4C
#define GS_REG_ZBUF_1			0x4E
#define GS_REG_XYOFFSET_1		0x18
#define GS_REG_SCISSOR_1		0x40
#define GS_REG_TEX0_1			0x06

//-----------------------------------------------------------------
// Registers for context 2
//-----------------------------------------------------------------
#define GS_REG_ALPHA_2			0x43
#define GS_REG_TEST_2			0x48
#define GS_REG_FRAME_2			0x4D
#define GS_REG_XYOFFSET_2		0x19
#define GS_REG_SCISSOR_2		0x41
#define GS_REG_ZBUF_2			0x4F
#define GS_REG_TEX0_2			0x07


//////////////////////////////////////////////////////////////
// PRIVILEGED REGISTERS
//////////////////////////////////////////////////////////////

#define GS_PMODE     	*((volatile unsigned long*)0x12000000)
#define GS_SMODE2      	*((volatile unsigned long*)0x12000020)
#define GS_DISPFB2      *((volatile unsigned long*)0x12000090)
#define GS_DISPLAY2     *((volatile unsigned long*)0x120000A0)
#define GS_BGCOLOR      *((volatile unsigned long*)0x120000E0)
#define GS_CSR      	*((volatile unsigned long*)0x12001000)
#define GS_BUSDIR		*((volatile unsigned long*)0x12001040)

// CSR
#define GS_CSR_RESET	((uint16)bit(9))

//////////////////////////////////////////////////////////////


#define GS_PRMODECONT_A_PRIM			0x1
#define GS_PRMODECONT_A_PRMODE			0x0

//-----------------------------------------------------------------
// PRIM SHADING
//-----------------------------------------------------------------
#define ITO_PRIM_SHADE_FLAT				0x0
#define ITO_PRIM_SHADE_GOURAUD			0x1

#define ITO_ZBUFFER_UPDATE				0
#define ITO_ZBUFFER_NEVER_UPDATE		1

#define ITO_FRAMEBUFFER_UPDATE			0
#define ITO_FRAMEBUFFER_NEVER_UPDATE	1

//-----------------------------------------------------------------
// Zbuffer Storage modes
//-----------------------------------------------------------------

#define ITO_ZBUF32						0
#define ITO_ZBUF24						1
#define ITO_ZBUF16						2
#define ITO_ZBUF16S						6

#define ITO_ZBUF32_SIZE					4
#define ITO_ZBUF24_SIZE					4
#define ITO_ZBUF16_SIZE					2

#define ITO_FRAME_1						0
#define ITO_FRAME_2						1

#define ITO_FRAMEBUFFER1				0
#define ITO_FRAMEBUFFER2				1

//-----------------------------------------------------------------
// Gif Packet types
//-----------------------------------------------------------------

#define ITO_GIFTAG_TYPE_LIST				0x0
#define ITO_GIFTAG_TYPE_IMAGE				0x2

//-----------------------------------------------------------------
// GifTag
//-----------------------------------------------------------------

#define ITO_GIFTAG_A_D					0xE
#define ITO_GIFTAG_PRIM				0x0


#define ITO_RGBA5551(r,g,b,a)			( (a << 15) | (b << 10) | (g << 5) | r )


#define ITO_RGB888(r,g,b)				((b << 16) | (g << 8) | r)	


#define ITO_RGBA(r,g,b,a)				((a << 24) | (b << 16) | (g << 8) | r)
#define ITO_RGBAQ(r,g,b,a,q)			( ((uint64)fmti(q) << 32) | (a << 24) | (b << 16) | (g << 8) | r)
#define ITO_Q(q)						((uint64)fmti(q) << 32) | 0


#define	ITO_RGBA32						0
#define ITO_RGB24						1
#define ITO_RGBA16						2
#define ITO_CLUT8						19
#define ITO_CLUT4						20

#define ITO_RGBA32_SIZE					4
#define ITO_RGBA24_SIZE					3
#define ITO_RGBA16_SIZE					2


#define PRMODE_PRIM						0x1

//#define attPRMODE						0x0

#define ITO_NON_INTERLACE				0
#define ITO_INTERLACE					1
#define	ITO_VMODE_NTSC					2
#define	ITO_VMODE_PAL					3
#define ITO_VMODE_AUTO					((*((char*)0x1FC7FF52))=='E')+2 
#define	ITO_FRAME						1
#define ITO_FIELD						0

//-----------------------------------------------------------------
// Pmode 
//-----------------------------------------------------------------

#define ALPHA_REG						0

#define BLEND_RC2						0

#define ITO_PRIM_TEXCOORDS_UV			1
#define ITO_PRIM_TEXCOORDS_STQ			0

//-----------------------------------------------------------------
// Texture width
//-----------------------------------------------------------------

#define ITO_TEXTURE_2					1
#define ITO_TEXTURE_4					2
#define ITO_TEXTURE_8					3
#define ITO_TEXTURE_16					4
#define ITO_TEXTURE_32					5
#define ITO_TEXTURE_64					6
#define ITO_TEXTURE_128					7
#define ITO_TEXTURE_256					8
#define ITO_TEXTURE_512					9
#define ITO_TEXTURE_1024				10


//-----------------------------------------------------------------
// TEST RELATED
//-----------------------------------------------------------------

//-----------------------------------------------------------------
// ALPHA
//-----------------------------------------------------------------

#define ITO_ALPHA_TEST_NEVER			0
#define ITO_ALPHA_TEST_ALWAYS			1
#define ITO_ALPHA_TEST_LESS				2
#define ITO_ALPHA_TEST_LEQUAL			3
#define ITO_ALPHA_TEST_EQUAL			4
#define ITO_ALPHA_TEST_GEQUAL			5
#define ITO_ALPHA_TEST_GREATER			6
#define ITO_ALPHA_TEST_NOTEQUAL			7

//-----------------------------------------------------------------
// ALPHA Failed
//-----------------------------------------------------------------

#define ITO_ALPHA_FAIL_KEEP				0
#define ITO_ALPHA_FAIL_FB_ONLY			1
#define ITO_ALPHA_FAIL_ZB_ONLY			2
#define ITO_ALPHA_FAIL_RGB_ONLY			3

//-----------------------------------------------------------------
// Z-Buffer Test
//-----------------------------------------------------------------

#define ITO_ZTEST_NEVER					0
#define ITO_ZTEST_ALWAYS				1
#define ITO_ZTEST_GEQUAL				2
#define ITO_ZTEST_GREATER				3

//-----------------------------------------------------------------
// TEXTURE LIGHT
//-----------------------------------------------------------------

#define ITO_LIGHT_MODULATE				0
#define ITO_LIGHT_DECAL					1
#define ITO_LIGHT_HIGH					2
#define ITO_LIGHT_HIGH2					3

//-----------------------------------------------------------------
// TEXUTRE COLOR COMPONET
//-----------------------------------------------------------------

#define ITO_COLORCOM_RGB				0
#define ITO_COLORCOM_RGBA				1

// ALPHA BLENDING

#define ITO_ALPHA_COLOR_SRC	0
#define ITO_ALPHA_COLOR_DST	1
#define ITO_ALPHA_ZERO		2

#define ITO_ALPHA_VALUE_SRC	0
#define ITO_ALPHA_VALUE_DST	1
#define ITO_ALPHA_FIXED		2


//////////////////////////////////////////////////////////////
// STRUCTURES
//////////////////////////////////////////////////////////////

typedef struct{
	uint16	x;
	uint16  y;
	
} itoGsContext;

typedef struct{
	uint8	psm;
	uint32	x;
	uint32  y;
} itoGsZBuffer;

typedef struct{
	uint16	width;
	uint16	height;
	uint8	psm;
	uint16	x;
	uint16	y;
} itoGsScreen;



typedef struct{
	
	itoGsScreen screen;
	itoGsContext	framebuffer1;
	itoGsContext	framebuffer2;
	itoGsZBuffer	zbuffer;

	uint32	scissor_x1;
	uint32	scissor_x2;
	uint32	scissor_y1;
	uint32	scissor_y2;

	uint8	dither;
	uint8	interlace;
	uint8	ffmode; // Frame/Field Mode
	uint8	vmode;
	
} itoGsEnv;

//////////////////////////////////////////////////////////////
// FUNCTIONS
//////////////////////////////////////////////////////////////



// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
//
// ito - free library for PlayStation 2 by Jules - www.mouthshut.net
//
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

// Double Buffer
void itoInitDoubleFrameBufferMode();
void itoSwitchFrameBuffers();

void itoSetActiveFrameBuffer(uint8 buffer);
void itoSetVisibleFrameBuffer(uint8 buffer);
uint8 itoGetVisibleFrameBuffer();
uint8 itoGetActiveFrameBuffer();

//-----------------------------------------------------------------
//-----------------------------------------------------------------
// PRIMS
//-----------------------------------------------------------------
//-----------------------------------------------------------------

//-----------------------------------------------------------------
// Trianglefan/strip & linestrip functions
//-----------------------------------------------------------------

void itoLineStrip(	uint64 color1, uint16 x1, uint16 y1, uint32 z1,
					uint64 color2, uint16 x2, uint16 y2, uint32 z2 );

void itoTriangleStrip(	uint64 color1, uint16 x1, uint16 y1, uint32 z1,
						uint64 color2, uint16 x2, uint16 y2, uint32 z2,
						uint64 color3, uint16 x3, uint16 y3, uint32 z3);

void itoTriangleFan(	uint64 color1, uint16 x1, uint16 y1, uint32 z1,
						uint64 color2, uint16 x2, uint16 y2, uint32 z2,
						uint64 color3, uint16 x3, uint16 y3, uint32 z3);

void itoTextureTriangleFanf(	uint64 color1, uint16 x1, uint16 y1, uint32 z1, float tx1, float ty1,
								uint64 color2, uint16 x2, uint16 y2, uint32 z2, float tx2, float ty2,
								uint64 color3, uint16 x3, uint16 y3, uint32 z3, float tx3, float ty3);

void itoTextureTriangleFan(	uint64 color1, uint16 x1, uint16 y1, uint32 z1, uint32 tx1, uint32 ty1,
							uint64 color2, uint16 x2, uint16 y2, uint32 z2, uint32 tx2, uint32 ty2,
							uint64 color3, uint16 x3, uint16 y3, uint32 z3, uint32 tx3, uint32 ty3);

void itoTextureTriangleStripf(	uint64 color1, uint16 x1, uint16 y1, uint32 z1, float tx1, float ty1,
								uint64 color2, uint16 x2, uint16 y2, uint32 z2, float tx2, float ty2,
								uint64 color3, uint16 x3, uint16 y3, uint32 z3, float tx3, float ty3);

void itoTextureTriangleStrip(	uint64 color1, uint16 x1, uint16 y1, uint32 z1, uint32 tx1, uint32 ty1,
								uint64 color2, uint16 x2, uint16 y2, uint32 z2, uint32 tx2, uint32 ty2,
								uint64 color3, uint16 x3, uint16 y3, uint32 z3, uint32 tx3, uint32 ty3);

void itoTextureLineStrip(	uint64 color1, uint16 x1, uint16 y1, uint32 z1, uint32 tx1, uint32 ty1,
							uint64 color2, uint16 x2, uint16 y2, uint32 z2, uint32 tx2, uint32 ty2 );

void itoTextureLineStripf(	uint64 color1, uint16 x1, uint16 y1, uint32 z1, float tx1, float ty1,
							uint64 color2, uint16 x2, uint16 y2, uint32 z2, float tx2, float ty2 );




//-----------------------------------------------------------------
// Vertex functions for trianglestrip/fan & linestrip
//-----------------------------------------------------------------

void itoAddVertex(uint64 color, uint16 x, uint16 y, uint32 z);
void itoAddTextureVertex(uint64 color, uint16 x, uint16 y, uint32 z, uint32 tx, uint32 ty);
void itoAddTextureVertexf(uint64 color, uint16 x, uint16 y, uint32 z, float tx, float ty);
void itoEndVertex();

//-----------------------------------------------------------------
// Sprite
//-----------------------------------------------------------------
void itoSprite(uint64 color, uint16 x1, uint16 y1, uint16 x2, uint16 y2, uint32 z);

void itoTextureSpritef(uint64 color,uint16 x1, uint16 y1, float tx1, float ty1, 
						uint16 x2, uint16 y2, float tx2, float ty2, uint32 z);

void itoTextureSprite(uint64 color, uint16 x1, uint16 y1, uint32 tx1, uint32 ty1, 
									uint16 x2, uint16 y2, uint32 tx2, uint32 ty2, uint32 z);
//-----------------------------------------------------------------
// Triangle
//-----------------------------------------------------------------
void itoTriangle(	uint64 color1, uint16 x1, uint16 y1, uint32 z1,
					uint64 color2, uint16 x2, uint16 y2, uint32 z2,
					uint64 color3, uint16 x3, uint16 y3, uint32 z3);

void itoTextureTrianglef(	uint64 color1, uint16 x1, uint16 y1, uint32 z1, float tx1, float ty1,
							uint64 color2, uint16 x2, uint16 y2, uint32 z2, float tx2, float ty2,
							uint64 color3, uint16 x3, uint16 y3, uint32 z3, float tx3, float ty3);

void itoTextureTriangle(uint64 color1, uint16 x1, uint16 y1, uint32 z1, uint32 tx1, uint32 ty1,
						uint64 color2, uint16 x2, uint16 y2, uint32 z2, uint32 tx2, uint32 ty2,
						uint64 color3, uint16 x3, uint16 y3, uint32 z3, uint32 tx3, uint32 ty3);

//-----------------------------------------------------------------
// Line
//-----------------------------------------------------------------
void itoLine(	uint64 color1, uint16 x1, uint16 y1, uint32 z1,
				uint64 color2, uint16 x2, uint16 y2, uint32 z2 );

void itoTextureLine(uint64 color1, uint16 x1, uint16 y1, uint32 z1, uint16 tx1, uint16 ty1,
					uint64 color2, uint16 x2, uint16 y2, uint32 z2, uint16 tx2, uint16 ty2 );

//-----------------------------------------------------------------
// Point
//-----------------------------------------------------------------
void itoPoint(uint64 color, uint16 x, uint16 y, uint32 z);
void itoTexturePoint(uint64 color, uint16 x, uint16 y, uint32 z, uint16 tx, uint16 ty);

//-----------------------------------------------------------------
// PRIM SETTINGS
//-----------------------------------------------------------------

void itoPrimAlphaBlending(uint8 use);
void itoPrimFog(uint8 use);
void itoPrimAntialiasing(uint8 use);
void itoPrimShade(uint8 mode);

void itoSetTextureCoords(uint8 mode);


//-----------------------------------------------------------------
// PRIVILEGED REGS
//-----------------------------------------------------------------

void itoPMODE(uint8 rc1, uint8 rc2, uint8 alpha_select, uint8 alpha_output, uint8 alpha_blend, uint8 alpha);
void itoDISPFB2(uint32 fbp, uint32 fbw, uint8 psm, uint16 dbx, uint16 dby);
void itoDISPLAY2(uint16 dx, uint16 dy, uint8 mag_x, uint8 mag_y, uint16 dw, uint16 dh);
void itoSMODE2(uint8 interlace, uint8 ffmode, uint8 vesa);

//-----------------------------------------------------------------
// GENERAL PURPOSE REGS
//-----------------------------------------------------------------

void itoFRAME(uint32 fb_base, uint32 fb_width, uint8 psm, uint32 update);
void itoFINISH();
void itoZBUF(uint32 zb_base, uint8 psm, uint8 update);
void itoXYOFFSET(uint32 offset_x, uint32 offset_y);
void itoSCISSOR(uint16 scissor_x1, uint16 scissor_y1, uint16 scissor_x2, uint16 scissor_y2);
void itoTEST( uint8 ate, uint8 atst, uint8 aref, uint8 afail, uint8 date, uint8 datm, uint8 zte, uint8 ztst);
void itoCOLCLAMP( uint64 colclamp);
void itoPRMODECONT( uint64 prmodecont);
void itoDITHER( uint64 dither);
void itoRGBAQ(uint64 rgbaq);
uint64 itoXYZ2(uint16 x, uint16 y, uint32 z);
void itoPRIM(uint8 prim, uint8 iip, uint8 tme, uint8 fge, uint8 abe, uint8 aai, uint8 fst, uint8 ctxt, uint8 fix);
void itoUV(uint16 u, uint16 v);
void itoTRXDIR(uint8 dir);
void itoTRXREG(uint16 width, uint16 height);
void itoTRXPOS(uint16 src_x,uint16 src_y,uint16 dst_x,uint16 dst_y, uint8 dir);
void itoBITBLTBUF(uint32 sbp, uint16 sbw, uint16 spsm, uint32 dbp, uint16 dbw, uint16 dpsm);
void itoTEX0(uint32 tbp0, uint16 tbw, uint8 psm,uint16 tw,uint16 th, uint8 tcc, uint8 tfx, uint32 cbp, uint8 cpsm,uint8 csm, uint8 csa, uint8 cld);
void itoTEXFLUSH();
void itoST(uint32 s, uint32 t);
void itoSTUV(uint32 tx, uint32 ty);
void itoTEXA(uint8 ta0, uint8 aem, uint8 ta1);
void itoFOG(uint8 f);
void itoFOGCOL(uint8 fcr, uint8 fcg, uint8 fcb);

//-----------------------------------------------------------------
// TEXTURE
//-----------------------------------------------------------------
void itoSetTexture(uint32 src, uint32 tbw, uint8 psm,uint16 w, uint16 h);
uint32 itoGetTextureBufferBase();
void itoSetTextureBufferBase(uint32 base);
void itoLoadTexture(void* src, uint32 dest, uint32 tbw, uint8 psm, uint16 x, uint16 y, uint16 w, uint16 h );
uint32 itoGetTextureSize(uint16 w, uint16 h, uint8 psm);
void itoTextureLight(uint8 mode);
void itoTextureColorComponet(uint8 mode);



//-----------------------------------------------------------------
// Framebuffers
//-----------------------------------------------------------------

uint32 itoGetFrameBufferBase(uint8 buffer);
void itoSetFrameBufferBase(uint8 buffer, uint32 base);
void itoFrameBufferUpdate(uint8 update);

//-----------------------------------------------------------------
// ZBUFFER
//-----------------------------------------------------------------
void itoZBufferUpdate(uint8 update);
uint32 itoGetZBufferBase();
void itoSetZBufferBase(uint32 base);

//-----------------------------------------------------------------
// TESTS
//-----------------------------------------------------------------
void itoAlphaTest(uint8 use, uint8 method, uint8 value, uint8 failed);
void itoZBufferTest(uint8 use, uint8 method);
void itoAlphaValue(uint8 use, uint8 a0, uint8 a1, uint8 tob);
void itoSetFogValue(uint8 fcr, uint8 fcg, uint8 fcb, uint8 f);
//-----------------------------------------------------------------
// GIF
//-----------------------------------------------------------------
void itoGifTag(uint16 nloop, uint8 eop, uint8 pre, uint16 prim, uint8 flag,uint8 nreg, uint64 regs);


//-----------------------------------------------------------------
// MISC
//-----------------------------------------------------------------

void itoGsEnvSubmit(itoGsEnv* env);

uint8 itoGetZPixelSize(uint8 mode);
uint8 itoGetPixelSize(uint8 mode);

void itoSetPrimXY(uint16 x, uint16 y);
void itoSetOffsetXY(uint16 x, uint16 y);
void itoGsReset();
void itoVSync();
void itoSetBgColor(uint32 color);
void itoSetScreenPos(uint16 x, uint16 y);
void itoSetScissor(uint16 x1, uint16 y1, uint16 x2, uint16 y2);

// ---------------- NEW


void itoSetClut( uint8 psm, uint32 base, uint16 offset);
void itoLoadClut(void* src, uint32 dest, uint16 cbw, uint8 psm, uint8 cpsm, uint16 x, uint16 y);

void itoMoveTexture(	uint8 psm, uint16 w, uint16 h,
						uint32 src,  uint32 s_tbw, uint16 s_x, uint16 s_y, 
						uint32 dest, uint32 d_tbw, uint16 d_x, uint16 d_y
						);

void itoBUSDIR( uint8 dir );
void itoSetAlphaBlending(uint8 a, uint8 b, uint8 c, uint8 d, uint8 fix);

#ifdef __cplusplus 
}
#endif /* __cplusplus */

#endif /* _ITOGS_H */
