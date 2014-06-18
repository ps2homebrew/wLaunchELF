//  ____     ___ |    / _____ _____
// |  __    |    |___/    |     |
// |___| ___|    |    \ __|__   |     gsKit Open Source Project.
// ----------------------------------------------------------------------
// Copyright 2004 - Chris "Neovanglist" Gilbert <Neovanglist@LainOS.org>
// Licenced under Academic Free License version 2.0
// Review gsKit README & LICENSE files for further details.
//
// gsTexture.c - Texture loading, handling, and manipulation.
//
// lzw() implimentation from jbit! Thanks jbit!
//

#include <stdio.h>
#include <string.h>
#include <kernel.h>

#include "gsKit.h"
#include "gsInline.h"

static inline u32 lzw(u32 val)
{
	u32 res;
	__asm__ __volatile__ ("   plzcw   %0, %1    " : "=r" (res) : "r" (val));
	return(res);
}

u32  gsKit_texture_size_ee(int width, int height, int psm)
{

#ifdef DEBUG
	printf("Width: %d - Height: %d\n", width, height);
#endif

	switch (psm) {
		case GS_PSM_CT32:  return (width*height*4);
		case GS_PSM_CT24:  return (width*height*4);
		case GS_PSM_CT16:  return (width*height*2);
		case GS_PSM_CT16S: return (width*height*2);
		case GS_PSM_T8:    return (width*height  );
		case GS_PSM_T4:    return (width*height/2);
		default: printf("gsKit: unsupported PSM %d\n", psm);
	}

	return -1;
}

u32  gsKit_texture_size(int width, int height, int psm)
{
	if(psm == GS_PSM_T8 || psm == GS_PSM_T4)

		width = (-GS_VRAM_TBWALIGN_CLUT)&(width+GS_VRAM_TBWALIGN_CLUT-1);
	else
		width = (-GS_VRAM_TBWALIGN)&(width+GS_VRAM_TBWALIGN-1);

	height = (-GS_VRAM_TBWALIGN)&(height+GS_VRAM_TBWALIGN-1);

#ifdef DEBUG
	printf("Width: %d - Height: %d\n", width, height);
#endif

	switch (psm) {
		case GS_PSM_CT32:  return (width*height*4);
		case GS_PSM_CT24:  return (width*height*4);
		case GS_PSM_CT16:  return (width*height*2);
		case GS_PSM_CT16S: return (width*height*2);
		case GS_PSM_T8:    return (width*height  );
		case GS_PSM_T4:    return (width*height/2);
		default: printf("gsKit: unsupported PSM %d\n", psm);
	}

	return -1;
}

void gsKit_texture_send(u32 *mem, int width, int height, u32 tbp, u32 psm, u32 tbw, u8 clut)
{
	u64* p_store;
	u64* p_data;
	u32* p_mem;
	int packets;
	int remain;
	int qwc;
	int p_size;

	qwc = gsKit_texture_size_ee(width, height, psm) / 16;
	if( gsKit_texture_size_ee(width, height, psm) % 16 )
	{
#ifdef GSKIT_DEBUG
		printf("Uneven division of quad word count from VRAM alloc. Rounding up QWC.\n");
#endif
		qwc++;
	}

	packets = qwc / GS_GIF_BLOCKSIZE;
	remain  = qwc % GS_GIF_BLOCKSIZE;
	p_mem   = (u32*)mem;

	if(clut == GS_CLUT_TEXTURE)
		p_size = (7+(packets * 3)+remain);
	else
		p_size = (9+(packets * 3)+remain);

	if(remain > 0)
		p_size += 2;

	p_store = p_data = memalign(64, (p_size * 16));

	FlushCache(0);

	// DMA DATA
	*p_data++ = DMA_TAG( 5, 0, DMA_CNT, 0, 0, 0 );
	*p_data++ = 0;

	*p_data++ = GIF_TAG( 4, 0, 0, 0, 0, 1 );
	*p_data++ = GIF_AD;

#ifdef GSKIT_DEBUG
	if(tbp % 256)
		printf("ERROR: Texture base pointer not on 256 byte alignment - Modulo = %d\n", tbp % 256);
#endif

	*p_data++ = GS_SETREG_BITBLTBUF(0, 0, 0, tbp/256, tbw, psm);
	*p_data++ = GS_BITBLTBUF;

	*p_data++ = GS_SETREG_TRXPOS(0, 0, 0, 0, 0);
	*p_data++ = GS_TRXPOS;

	*p_data++ = GS_SETREG_TRXREG(width, height);
	*p_data++ = GS_TRXREG;

	*p_data++ = GS_SETREG_TRXDIR(0);
	*p_data++ = GS_TRXDIR;

	while (packets-- > 0)
	{
		*p_data++ = DMA_TAG( 1, 0, DMA_CNT, 0, 0, 0);
		*p_data++ = 0;
		*p_data++ = GIF_TAG( GS_GIF_BLOCKSIZE, 0, 0, 0, 2, 0 );
		*p_data++ = 0;
		*p_data++ = DMA_TAG( GS_GIF_BLOCKSIZE, 1, DMA_REF, 0, (u32)p_mem, 0 );
		*p_data++ = 0;
		p_mem+= (GS_GIF_BLOCKSIZE * 4);
	}

	if (remain > 0) {
		*p_data++ = DMA_TAG( 1, 0, DMA_CNT, 0, 0, 0);
		*p_data++ = 0;
		*p_data++ = GIF_TAG( remain, 0, 0, 0, 2, 0 );
		*p_data++ = 0;
		*p_data++ = DMA_TAG( remain, 1, DMA_REF, 0, (u32)p_mem, 0 );
		*p_data++ = 0;
	}

	if(clut == GS_CLUT_TEXTURE)
	{
		*p_data++ = DMA_TAG( 0, 0, DMA_END, 0, 0, 0 );
		*p_data++ = 0;
	}
	else
	{
		*p_data++ = DMA_TAG( 2, 0, DMA_END, 0, 0, 0 );
		*p_data++ = 0;

		*p_data++ = GIF_TAG( 1, 1, 0, 0, 0, 1 );
		*p_data++ = GIF_AD;

		*p_data++ = 0;
		*p_data++ = GS_TEXFLUSH;
	}

	// Need to wait first to make sure that if our doublebuffered drawbuffer is still
	// chugging away we have a safe time to start our transfer.
	dmaKit_wait_fast();
	dmaKit_send_chain(DMA_CHANNEL_GIF, p_store, p_size);
	free(p_store);

}

void gsKit_texture_send_inline(GSGLOBAL *gsGlobal, u32 *mem, int width, int height, u32 tbp, u32 psm, u32 tbw, u8 clut)
{
	u64* p_data;
	u32* p_mem;
	int packets;
	int remain;
	int qwc;
	int dmasize;

	qwc = gsKit_texture_size_ee(width, height, psm) / 16;
	if( gsKit_texture_size_ee(width, height, psm) % 16 )
	{
#ifdef GSKIT_DEBUG
		printf("Uneven division of quad word count from VRAM alloc. Rounding up QWC.\n");
#endif
		qwc++;
	}

	packets = qwc / GS_GIF_BLOCKSIZE;
	remain  = qwc % GS_GIF_BLOCKSIZE;
	p_mem   = (u32*)mem;

	p_data = gsKit_heap_alloc(gsGlobal, 4, 64, GIF_AD);

	*p_data++ = GIF_TAG_AD(4);
	*p_data++ = GIF_AD;

#ifdef GSKIT_DEBUG
	if(tbp % 256)
		printf("ERROR: Texture base pointer not on 256 byte alignment - Modulo = %d\n", tbp % 256);
#endif

	*p_data++ = GS_SETREG_BITBLTBUF(0, 0, 0, tbp/256, tbw, psm);
	*p_data++ = GS_BITBLTBUF;

	*p_data++ = GS_SETREG_TRXPOS(0, 0, 0, 0, 0);
	*p_data++ = GS_TRXPOS;

	*p_data++ = GS_SETREG_TRXREG(width, height);
	*p_data++ = GS_TRXREG;

	*p_data++ = GS_SETREG_TRXDIR(0);
	*p_data++ = GS_TRXDIR;

	dmasize = (packets * 3);
	if(remain)
		dmasize += 3;

	p_data = gsKit_heap_alloc_dma(gsGlobal, dmasize, dmasize * 16);

	while (packets-- > 0)
	{
		*p_data++ = DMA_TAG( 1, 0, DMA_CNT, 0, 0, 0);
		*p_data++ = 0;
		*p_data++ = GIF_TAG( GS_GIF_BLOCKSIZE, 0, 0, 0, 2, 0 );
		*p_data++ = 0;
		*p_data++ = DMA_TAG( GS_GIF_BLOCKSIZE, 0, DMA_REF, 0, (u32)p_mem, 0 );
		*p_data++ = 0;
		p_mem+= (GS_GIF_BLOCKSIZE * 4);
	}

	if (remain > 0) {
		*p_data++ = DMA_TAG( 1, 0, DMA_CNT, 0, 0, 0);
		*p_data++ = 0;
		*p_data++ = GIF_TAG( remain, 0, 0, 0, 2, 0 );
		*p_data++ = 0;
		*p_data++ = DMA_TAG( remain, 0, DMA_REF, 0, (u32)p_mem, 0 );
		*p_data++ = 0;
	}

	if(clut != GS_CLUT_TEXTURE)
	{
		p_data = gsKit_heap_alloc(gsGlobal, 1, 16, GIF_AD);

		*p_data++ = GIF_TAG_AD(1);
		*p_data++ = GIF_AD;

		*p_data++ = 0;
		*p_data++ = GS_TEXFLUSH;
	}

}

void gsKit_texture_upload(GSGLOBAL *gsGlobal, GSTEXTURE *Texture)
{
	gsKit_setup_tbw(Texture);

	if (Texture->PSM == GS_PSM_T8)
	{
		gsKit_texture_send(Texture->Mem, Texture->Width, Texture->Height, Texture->Vram, Texture->PSM, Texture->TBW, GS_CLUT_TEXTURE);
		gsKit_texture_send(Texture->Clut, 16, 16, Texture->VramClut, Texture->ClutPSM, 1, GS_CLUT_PALLETE);

	}
	else if (Texture->PSM == GS_PSM_T4)
	{
		gsKit_texture_send(Texture->Mem, Texture->Width, Texture->Height, Texture->Vram, Texture->PSM, Texture->TBW, GS_CLUT_TEXTURE);
		gsKit_texture_send(Texture->Clut, 8,  2, Texture->VramClut, Texture->ClutPSM, 1, GS_CLUT_PALLETE);
	}
	else
	{
		gsKit_texture_send(Texture->Mem, Texture->Width, Texture->Height, Texture->Vram, Texture->PSM, Texture->TBW, GS_CLUT_NONE);
	}
}

void gsKit_prim_sprite_texture_3d(GSGLOBAL *gsGlobal, const GSTEXTURE *Texture,
				float x1, float y1, int iz1, float u1, float v1,
				float x2, float y2, int iz2, float u2, float v2, u64 color)
{
	gsKit_set_texfilter(gsGlobal, Texture->Filter);

	u64* p_store;
	u64* p_data;
	int qsize = 4;
	int bsize = 64;

	if(gsGlobal->Field == GS_FRAME)
	{
		y1 /= 2.0f;
		y2 /= 2.0f;
#ifdef GSKIT_ENABLE_HBOFFSET
		if(!gsGlobal->EvenOrOdd)
		{
			y1 += 0.5f;
			y2 += 0.5f;
		}
#endif
	}

	int ix1,ix2,iy1,iy2,iu1,iu2,iv1,iv2;

	if(x1<x2){
		ix1 = (int)(x1 * 16.0f) + gsGlobal->OffsetX;
		ix2 = (int)((x2 + 0.0625f) * 16.0f) + gsGlobal->OffsetX;
	}else{
		ix2 = (int)(x2 * 16.0f) + gsGlobal->OffsetX;
		ix1 = (int)((x1 + 0.0625f) * 16.0f) + gsGlobal->OffsetX;
	}

	if(y1<y2){
		iy1 = (int)(y1 * 16.0f) + gsGlobal->OffsetY;
		iy2 = (int)((y2 + 0.0625f) * 16.0f) + gsGlobal->OffsetY;
	}else{
		iy2 = (int)(y2 * 16.0f) + gsGlobal->OffsetY;
		iy1 = (int)((y1 + 0.0625f) * 16.0f) + gsGlobal->OffsetY;
	}

	if(u1<u2){
		iu1 = (int)((u1 + 0.5f) * 16.0f);
		iu2 = (int)((u2 - 0.375f) * 16.0f);
	}else{
		iu2 = (int)((u2 + 0.5f) * 16.0f);
		iu1 = (int)((u1 - 0.375f) * 16.0f);
	}

	if(v1<v2){
		iv1 = (int)((v1 + 0.5f) * 16.0f);
		iv2 = (int)((v2 - 0.375f) * 16.0f);
	}else{
		iv2 = (int)((v2 + 0.5f) * 16.0f);
		iv1 = (int)((v1 - 0.375f) * 16.0f);
	}

	int tw = 31 - (lzw(Texture->Width) + 1);
	if(Texture->Width > (1<<tw))
		tw++;

	int th = 31 - (lzw(Texture->Height) + 1);
	if(Texture->Height > (1<<th))
		th++;

	p_store = p_data = gsKit_heap_alloc(gsGlobal, qsize, bsize, GIF_PRIM_SPRITE_TEXTURED);

	*p_data++ = GIF_TAG_SPRITE_TEXTURED(0);
	*p_data++ = GIF_TAG_SPRITE_TEXTURED_REGS(gsGlobal->PrimContext);

	if(Texture->VramClut == 0)
	{
		*p_data++ = GS_SETREG_TEX0(Texture->Vram/256, Texture->TBW, Texture->PSM,
			tw, th, gsGlobal->PrimAlphaEnable, 0,
			0, 0, Texture->ClutPSM, 0, GS_CLUT_STOREMODE_NOLOAD);
	}
	else
	{
		*p_data++ = GS_SETREG_TEX0(Texture->Vram/256, Texture->TBW, Texture->PSM,
			tw, th, gsGlobal->PrimAlphaEnable, 0,
			Texture->VramClut/256, 0, 0, 0, GS_CLUT_STOREMODE_LOAD);
	}

	*p_data++ = GS_SETREG_PRIM( GS_PRIM_PRIM_SPRITE, 0, 1, gsGlobal->PrimFogEnable,
				gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable,
				1, gsGlobal->PrimContext, 0);

	*p_data++ = color;

	*p_data++ = GS_SETREG_UV( iu1, iv1 );
	*p_data++ = GS_SETREG_XYZ2( ix1, iy1, iz1 );

	*p_data++ = GS_SETREG_UV( iu2, iv2 );
	*p_data++ = GS_SETREG_XYZ2( ix2, iy2, iz2 );
}

void gsKit_prim_sprite_striped_texture_3d(GSGLOBAL *gsGlobal, const GSTEXTURE *Texture,
				float x1, float y1, int iz1, float u1, float v1,
				float x2, float y2, int iz2, float u2, float v2, u64 color)
{
	// If you do bilinear on this the results will be a disaster because of bleeding.
	gsKit_set_texfilter(gsGlobal, GS_FILTER_NEAREST);

	u64* p_store;
	u64* p_data;
	int qsize = 4;
	int bsize = 64;

	int ix1 = x1;
	int ix2 = x2;
	int iy1 = (int)(y1 * 16.0f) + gsGlobal->OffsetY;
	int iy2 = (int)(y2 * 16.0f) + gsGlobal->OffsetY;

	int iv1 = (int)(v1 * 16.0f);
	int iv2 = (int)(v2 * 16.0f);

	int tw = 31 - (lzw(Texture->Width) + 1);
	if(Texture->Width > (1<<tw))
		tw++;

	int th = 31 - (lzw(Texture->Height) + 1);
	if(Texture->Height > (1<<th))
		th++;

	u64 Tex0;
	if(!Texture->VramClut)
	{
		Tex0 = GS_SETREG_TEX0(Texture->Vram/256, Texture->TBW, Texture->PSM,
			tw, th, gsGlobal->PrimAlphaEnable, 0,
			0, 0, Texture->ClutPSM, 0, GS_CLUT_STOREMODE_NOLOAD);
	}
	else
	{
		Tex0 = GS_SETREG_TEX0(Texture->Vram/256, Texture->TBW, Texture->PSM,
			tw, th, gsGlobal->PrimAlphaEnable, 0,
			Texture->VramClut/256, 0, 0, 0, GS_CLUT_STOREMODE_LOAD);
	}

	u64 Prim = GS_SETREG_PRIM( GS_PRIM_PRIM_SPRITE, 0, 1, gsGlobal->PrimFogEnable,
				gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable,
				1, gsGlobal->PrimContext, 0);

	ix1 = ((ix1 >> 6) << 6);
	ix2 = ((ix2 >> 6) << 6);

	int extracount = 0;
	if(((int)x1) % 64)
	{
		ix1 += 64;
		extracount++;
	}
	if(((int)x2) % 64)
	{
		extracount++;
	}

	int stripcount = ((ix2 - ix1) / 64);

	if(!(stripcount + extracount))
	{
		return;
	}

	float leftwidth = ix1 - (float)x1;
	float rightwidth = (float)x2 - ix2;

	float ustripwidth;

	float leftuwidth = ((leftwidth / (x2-x1)) * (u2 - u1));

	float rightuwidth = ((rightwidth / (x2-x1)) * (u2 - u1));

	ustripwidth = ((((u2 - u1) - leftuwidth) - rightuwidth) / stripcount);

	float fu1 = 0.0f;

	// This is a lie btw... it's not really A+D mode, but I lie to my allocator so it doesn't screw up the NLOOP arg of the GIFTAG.
	p_store = p_data = gsKit_heap_alloc(gsGlobal, qsize * (stripcount + extracount), bsize * (stripcount + extracount), GIF_AD);

	*p_data++ = GIF_TAG_SPRITE_TEXTURED((stripcount + extracount));
	*p_data++ = GIF_TAG_SPRITE_TEXTURED_REGS(gsGlobal->PrimContext);

	//printf("stripcount = %d | extracount = %d | ustripwidth = %f | rightwidth = %f\n", stripcount, extracount, ustripwidth, rightwidth);

	if(leftwidth >= 1.0f)
	{
		*p_data++ = Tex0;
		*p_data++ = Prim;

		*p_data++ = color;

		*p_data++ = GS_SETREG_UV( ((int)(u1 * 16.0f)), iv1 );
		*p_data++ = GS_SETREG_XYZ2( ((int)(x1 * 16.0f) + gsGlobal->OffsetX), iy1, iz1 );

		fu1 = leftuwidth;

		*p_data++ = GS_SETREG_UV( ((int)(fu1 * 16.0f)), iv2 );
		*p_data++ = GS_SETREG_XYZ2( (ix1 << 4) + gsGlobal->OffsetX, iy2, iz2 );

		*p_data++ = 0;
	}

	while(stripcount--)
	{
		*p_data++ = Tex0;
		*p_data++ = Prim;

		*p_data++ = color;

		*p_data++ = GS_SETREG_UV(((int)(fu1 * 16.0f)), iv1);
		*p_data++ = GS_SETREG_XYZ2((ix1 << 4) + gsGlobal->OffsetX, iy1, iz1 );

		fu1 += ustripwidth;
		ix1 += 64;

		*p_data++ = GS_SETREG_UV(((int)(fu1 * 16.0f)), iv2);
		*p_data++ = GS_SETREG_XYZ2((ix1 << 4) + gsGlobal->OffsetX, iy2, iz2 );

		*p_data++ = 0;
	}


	if(rightwidth > 0.0f)
	{

		*p_data++ = Tex0;
		*p_data++ = Prim;

		*p_data++ = color;

		*p_data++ = GS_SETREG_UV(((int)(fu1 * 16.0f)), iv1);
		*p_data++ = GS_SETREG_XYZ2((ix1 << 4) + gsGlobal->OffsetX, iy1, iz1 );

		fu1 += rightuwidth;
		rightwidth += ix1;

		*p_data++ = GS_SETREG_UV(((int)(fu1 * 16.0f)), iv2);
		*p_data++ = GS_SETREG_XYZ2(((int)(rightwidth * 16.0f) + gsGlobal->OffsetX), iy2, iz2 );

		*p_data++ = 0;

	}

}

void gsKit_prim_triangle_texture_3d(GSGLOBAL *gsGlobal, GSTEXTURE *Texture,
				float x1, float y1, int iz1, float u1, float v1,
				float x2, float y2, int iz2, float u2, float v2,
				float x3, float y3, int iz3, float u3, float v3, u64 color)
{
	gsKit_set_texfilter(gsGlobal, Texture->Filter);
	u64* p_store;
	u64* p_data;
	int qsize = 5;
	int bsize = 80;

	int tw = 31 - (lzw(Texture->Width) + 1);
	if(Texture->Width > (1<<tw))
		tw++;

	int th = 31 - (lzw(Texture->Height) + 1);
	if(Texture->Height > (1<<th))
		th++;

	int ix1 = (int)(x1 * 16.0f) + gsGlobal->OffsetX;
	int ix2 = (int)(x2 * 16.0f) + gsGlobal->OffsetX;
	int ix3 = (int)(x3 * 16.0f) + gsGlobal->OffsetX;
	int iy1 = (int)(y1 * 16.0f) + gsGlobal->OffsetY;
	int iy2 = (int)(y2 * 16.0f) + gsGlobal->OffsetY;
	int iy3 = (int)(y3 * 16.0f) + gsGlobal->OffsetY;

	int iu1 = (int)(u1 * 16.0f);
	int iu2 = (int)(u2 * 16.0f);
	int iu3 = (int)(u3 * 16.0f);
	int iv1 = (int)(v1 * 16.0f);
	int iv2 = (int)(v2 * 16.0f);
	int iv3 = (int)(v3 * 16.0f);

	p_store = p_data = gsKit_heap_alloc(gsGlobal, qsize, bsize, GIF_PRIM_TRIANGLE_TEXTURED);

	*p_data++ = GIF_TAG_TRIANGLE_TEXTURED(0);
	*p_data++ = GIF_TAG_TRIANGLE_TEXTURED_REGS(gsGlobal->PrimContext);

	if(Texture->VramClut == 0)
	{
		*p_data++ = GS_SETREG_TEX0(Texture->Vram/256, Texture->TBW, Texture->PSM,
			tw, th, gsGlobal->PrimAlphaEnable, 0,
			0, 0, Texture->ClutPSM, 0, GS_CLUT_STOREMODE_NOLOAD);
	}
	else
	{
		*p_data++ = GS_SETREG_TEX0(Texture->Vram/256, Texture->TBW, Texture->PSM,
			tw, th, gsGlobal->PrimAlphaEnable, 0,
			Texture->VramClut/256, 0, 0, 0, GS_CLUT_STOREMODE_LOAD);
	}

	*p_data++ = GS_SETREG_PRIM( GS_PRIM_PRIM_TRIANGLE, 0, 1, gsGlobal->PrimFogEnable,
				gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable,
				1, gsGlobal->PrimContext, 0);


	*p_data++ = color;

	*p_data++ = GS_SETREG_UV( iu1, iv1 );
	*p_data++ = GS_SETREG_XYZ2( ix1, iy1, iz1 );

	*p_data++ = GS_SETREG_UV( iu2, iv2 );
	*p_data++ = GS_SETREG_XYZ2( ix2, iy2, iz2 );

	*p_data++ = GS_SETREG_UV( iu3, iv3 );
	*p_data++ = GS_SETREG_XYZ2( ix3, iy3, iz3 );
}
void gsKit_prim_triangle_goraud_texture_3d(GSGLOBAL *gsGlobal, GSTEXTURE *Texture,
				float x1, float y1, int iz1, float u1, float v1,
				float x2, float y2, int iz2, float u2, float v2,
				float x3, float y3, int iz3, float u3, float v3,
				u64 color1, u64 color2, u64 color3)
{
	gsKit_set_texfilter(gsGlobal, Texture->Filter);
	u64* p_store;
	u64* p_data;
	int qsize = 6;
	int bsize = 96;

	int tw = 31 - (lzw(Texture->Width) + 1);
	if(Texture->Width > (1<<tw))
		tw++;

	int th = 31 - (lzw(Texture->Height) + 1);
	if(Texture->Height > (1<<th))
		th++;

	int ix1 = (int)(x1 * 16.0f) + gsGlobal->OffsetX;
	int ix2 = (int)(x2 * 16.0f) + gsGlobal->OffsetX;
	int ix3 = (int)(x3 * 16.0f) + gsGlobal->OffsetX;
	int iy1 = (int)(y1 * 16.0f) + gsGlobal->OffsetY;
	int iy2 = (int)(y2 * 16.0f) + gsGlobal->OffsetY;
	int iy3 = (int)(y3 * 16.0f) + gsGlobal->OffsetY;

	int iu1 = (int)(u1 * 16.0f);
	int iu2 = (int)(u2 * 16.0f);
	int iu3 = (int)(u3 * 16.0f);
	int iv1 = (int)(v1 * 16.0f);
	int iv2 = (int)(v2 * 16.0f);
	int iv3 = (int)(v3 * 16.0f);

	p_store = p_data = gsKit_heap_alloc(gsGlobal, qsize, bsize, GIF_PRIM_TRIANGLE_TEXTURED);

	*p_data++ = GIF_TAG_TRIANGLE_GORAUD_TEXTURED(0);
	*p_data++ = GIF_TAG_TRIANGLE_GORAUD_TEXTURED_REGS(gsGlobal->PrimContext);

	if(Texture->VramClut == 0)
	{
		*p_data++ = GS_SETREG_TEX0(Texture->Vram/256, Texture->TBW, Texture->PSM,
			tw, th, gsGlobal->PrimAlphaEnable, 0,
			0, 0, Texture->ClutPSM, 0, GS_CLUT_STOREMODE_NOLOAD);
	}
	else
	{
		*p_data++ = GS_SETREG_TEX0(Texture->Vram/256, Texture->TBW, Texture->PSM,
			tw, th, gsGlobal->PrimAlphaEnable, 0,
			Texture->VramClut/256, 0, 0, 0, GS_CLUT_STOREMODE_LOAD);
	}

	*p_data++ = GS_SETREG_PRIM( GS_PRIM_PRIM_TRIANGLE, 1, 1, gsGlobal->PrimFogEnable,
				gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable,
				1, gsGlobal->PrimContext, 0);


	*p_data++ = color1;
	*p_data++ = GS_SETREG_UV( iu1, iv1 );
	*p_data++ = GS_SETREG_XYZ2( ix1, iy1, iz1 );

  *p_data++ = color2;
	*p_data++ = GS_SETREG_UV( iu2, iv2 );
	*p_data++ = GS_SETREG_XYZ2( ix2, iy2, iz2 );

  *p_data++ = color3;
	*p_data++ = GS_SETREG_UV( iu3, iv3 );
	*p_data++ = GS_SETREG_XYZ2( ix3, iy3, iz3 );
}
void gsKit_prim_triangle_strip_texture(GSGLOBAL *gsGlobal, GSTEXTURE *Texture,
					float *TriStrip, int segments, int iz, u64 color)
{
	gsKit_set_texfilter(gsGlobal, Texture->Filter);
	u64* p_store;
	u64* p_data;
	int qsize = 3 + (segments * 2);
	int count;
	int vertexdata[segments*4];

	int tw = 31 - (lzw(Texture->Width) + 1);
	if(Texture->Width > (1<<tw))
		tw++;

	int th = 31 - (lzw(Texture->Height) + 1);
	if(Texture->Height > (1<<th))
		th++;

	for(count = 0; count < (segments * 4); count+=4)
	{
		vertexdata[count] =  (int)((*TriStrip++) * 16.0f) + gsGlobal->OffsetX;
		vertexdata[count+1] =  (int)((*TriStrip++) * 16.0f) + gsGlobal->OffsetY;
		vertexdata[count+2] =  (int)((*TriStrip++) * 16.0f);
		vertexdata[count+3] =  (int)((*TriStrip++) * 16.0f);
	}

	p_store = p_data = gsKit_heap_alloc(gsGlobal, qsize, (qsize * 16), GIF_AD);

	*p_data++ = GIF_TAG_AD(qsize);
	*p_data++ = GIF_AD;

	if(Texture->VramClut == 0)
	{
		*p_data++ = GS_SETREG_TEX0(Texture->Vram/256, Texture->TBW, Texture->PSM,
			tw, th, gsGlobal->PrimAlphaEnable, 0,
			0, 0, Texture->ClutPSM, 0, GS_CLUT_STOREMODE_NOLOAD);
	}
	else
	{
		*p_data++ = GS_SETREG_TEX0(Texture->Vram/256, Texture->TBW, Texture->PSM,
			tw, th, gsGlobal->PrimAlphaEnable, 0,
			Texture->VramClut/256, 0, 0, 0, GS_CLUT_STOREMODE_LOAD);
	}
	*p_data++ = GS_TEX0_1+gsGlobal->PrimContext;

	*p_data++ = GS_SETREG_PRIM( GS_PRIM_PRIM_TRISTRIP, 0, 1, gsGlobal->PrimFogEnable,
				gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable,
				1, gsGlobal->PrimContext, 0);

	*p_data++ = GS_PRIM;

	*p_data++ = color;
	*p_data++ = GS_RGBAQ;

	for(count = 0; count < (segments * 4); count+=4)
	{
		*p_data++ = GS_SETREG_UV( vertexdata[count+2], vertexdata[count+3] );
		*p_data++ = GS_UV;

		*p_data++ = GS_SETREG_XYZ2( vertexdata[count], vertexdata[count+1], iz );
		*p_data++ = GS_XYZ2;
	}

}

void gsKit_prim_triangle_strip_texture_3d(GSGLOBAL *gsGlobal, GSTEXTURE *Texture,
					float *TriStrip, int segments, u64 color)
{
	gsKit_set_texfilter(gsGlobal, Texture->Filter);
	u64* p_store;
	u64* p_data;
	int qsize = 3 + (segments * 2);
	int count;
	int vertexdata[segments*5];

	int tw = 31 - (lzw(Texture->Width) + 1);
	if(Texture->Width > (1<<tw))
		tw++;

	int th = 31 - (lzw(Texture->Height) + 1);
	if(Texture->Height > (1<<th))
		th++;

	for(count = 0; count < (segments * 5); count+=5)
	{
		vertexdata[count] = (int)((*TriStrip++) * 16.0f) + gsGlobal->OffsetX;
		vertexdata[count+1] = (int)((*TriStrip++) * 16.0f) + gsGlobal->OffsetY;
		vertexdata[count+2] = (int)((*TriStrip++) * 16.0f);
		vertexdata[count+3] = (int)((*TriStrip++) * 16.0f);
		vertexdata[count+4] = (int)((*TriStrip++) * 16.0f);
	}

	p_store = p_data = gsKit_heap_alloc(gsGlobal, qsize, (qsize * 16), GIF_AD);

	*p_data++ = GIF_TAG_AD(qsize);
	*p_data++ = GIF_AD;

	if(Texture->VramClut == 0)
	{
		*p_data++ = GS_SETREG_TEX0(Texture->Vram/256, Texture->TBW, Texture->PSM,
			tw, th, gsGlobal->PrimAlphaEnable, 0,
			0, 0, Texture->ClutPSM, 0, GS_CLUT_STOREMODE_NOLOAD);
	}
	else
	{
		*p_data++ = GS_SETREG_TEX0(Texture->Vram/256, Texture->TBW, Texture->PSM,
			tw, th, gsGlobal->PrimAlphaEnable, 0,
			Texture->VramClut/256, 0, 0, 0, GS_CLUT_STOREMODE_LOAD);
	}
	*p_data++ = GS_TEX0_1+gsGlobal->PrimContext;

	*p_data++ = GS_SETREG_PRIM( GS_PRIM_PRIM_TRISTRIP, 0, 1, gsGlobal->PrimFogEnable,
				gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable,
				1, gsGlobal->PrimContext, 0);

	*p_data++ = GS_PRIM;

	*p_data++ = color;
	*p_data++ = GS_RGBAQ;

	for(count = 0; count < (segments * 5); count+=5)
	{
		*p_data++ = GS_SETREG_UV( vertexdata[count+3], vertexdata[count+4] );
		*p_data++ = GS_UV;

		*p_data++ = GS_SETREG_XYZ2( vertexdata[count], vertexdata[count+1], vertexdata[count+2] );
		*p_data++ = GS_XYZ2;
	}
}

void gsKit_prim_triangle_fan_texture(GSGLOBAL *gsGlobal, GSTEXTURE *Texture,
					float *TriFan, int verticies, int iz, u64 color)
{
	gsKit_set_texfilter(gsGlobal, Texture->Filter);
	u64* p_store;
	u64* p_data;
	int qsize = 3 + (verticies * 2);
	int count;
	int vertexdata[verticies*4];

	int tw = 31 - (lzw(Texture->Width) + 1);
	if(Texture->Width > (1<<tw))
		tw++;

	int th = 31 - (lzw(Texture->Height) + 1);
	if(Texture->Height > (1<<th))
		th++;

	for(count = 0; count < (verticies * 4); count+=4)
	{
		vertexdata[count] =  (int)((*TriFan++) * 16.0f) + gsGlobal->OffsetX;
		vertexdata[count+1] =  (int)((*TriFan++) * 16.0f) + gsGlobal->OffsetY;
		vertexdata[count+2] =  (int)((*TriFan++) * 16.0f);
		vertexdata[count+3] =  (int)((*TriFan++) * 16.0f);
	}

	p_store = p_data = gsKit_heap_alloc(gsGlobal, qsize, (qsize * 16), GIF_AD);

	*p_data++ = GIF_TAG_AD(qsize);
	*p_data++ = GIF_AD;

	if(Texture->VramClut == 0)
	{
		*p_data++ = GS_SETREG_TEX0(Texture->Vram/256, Texture->TBW, Texture->PSM,
			tw, th, gsGlobal->PrimAlphaEnable, 0,
			0, 0, Texture->ClutPSM, 0, GS_CLUT_STOREMODE_NOLOAD);
	}
	else
	{
		*p_data++ = GS_SETREG_TEX0(Texture->Vram/256, Texture->TBW, Texture->PSM,
			tw, th, gsGlobal->PrimAlphaEnable, 0,
			Texture->VramClut/256, 0, 0, 0, GS_CLUT_STOREMODE_LOAD);
	}
	*p_data++ = GS_TEX0_1+gsGlobal->PrimContext;

	*p_data++ = GS_SETREG_PRIM( GS_PRIM_PRIM_TRIFAN, 0, 1, gsGlobal->PrimFogEnable,
				gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable,
				1, gsGlobal->PrimContext, 0);

	*p_data++ = GS_PRIM;

	*p_data++ = color;
	*p_data++ = GS_RGBAQ;

	for(count = 0; count < (verticies * 4); count+=4)
	{
		*p_data++ = GS_SETREG_UV( vertexdata[count+2], vertexdata[count+3] );
		*p_data++ = GS_UV;

		*p_data++ = GS_SETREG_XYZ2( vertexdata[count], vertexdata[count+1], iz );
		*p_data++ = GS_XYZ2;
	}
}

void gsKit_prim_triangle_fan_texture_3d(GSGLOBAL *gsGlobal, GSTEXTURE *Texture,
					float *TriFan, int verticies, u64 color)
{
	gsKit_set_texfilter(gsGlobal, Texture->Filter);
	u64* p_store;
	u64* p_data;
	int qsize = 3 + (verticies * 2);
	int count;
	int vertexdata[verticies*5];

	int tw = 31 - (lzw(Texture->Width) + 1);
	if(Texture->Width > (1<<tw))
		tw++;

	int th = 31 - (lzw(Texture->Height) + 1);
	if(Texture->Height > (1<<th))
		th++;

	for(count = 0; count < (verticies * 5); count+=5)
	{
		vertexdata[count] =  (int)((*TriFan++) * 16.0f) + gsGlobal->OffsetX;
		vertexdata[count+1] =  (int)((*TriFan++) * 16.0f) + gsGlobal->OffsetY;
		vertexdata[count+2] =  (int)((*TriFan++) * 16.0f);
		vertexdata[count+3] =  (int)((*TriFan++) * 16.0f);
		vertexdata[count+4] =  (int)((*TriFan++) * 16.0f);
	}

	p_store = p_data = gsKit_heap_alloc(gsGlobal, qsize, (qsize * 16), GIF_AD);

	*p_data++ = GIF_TAG_AD(qsize);
	*p_data++ = GIF_AD;

	if(Texture->VramClut == 0)
	{
		*p_data++ = GS_SETREG_TEX0(Texture->Vram/256, Texture->TBW, Texture->PSM,
			tw, th, gsGlobal->PrimAlphaEnable, 0,
			0, 0, Texture->ClutPSM, 0, GS_CLUT_STOREMODE_NOLOAD);
	}
	else
	{
		*p_data++ = GS_SETREG_TEX0(Texture->Vram/256, Texture->TBW, Texture->PSM,
			tw, th, gsGlobal->PrimAlphaEnable, 0,
			Texture->VramClut/256, 0, 0, 0, GS_CLUT_STOREMODE_LOAD);
	}
	*p_data++ = GS_TEX0_1+gsGlobal->PrimContext;

	*p_data++ = GS_SETREG_PRIM( GS_PRIM_PRIM_TRIFAN, 0, 1, gsGlobal->PrimFogEnable,
				gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable,
				1, gsGlobal->PrimContext, 0);

	*p_data++ = GS_PRIM;

	*p_data++ = color;
	*p_data++ = GS_RGBAQ;

	for(count = 0; count < (verticies * 5); count+=5)
	{
		*p_data++ = GS_SETREG_UV( vertexdata[count+3], vertexdata[count+4] );
		*p_data++ = GS_UV;

		*p_data++ = GS_SETREG_XYZ2( vertexdata[count], vertexdata[count+1], vertexdata[count+2] );
		*p_data++ = GS_XYZ2;
	}
}

void gsKit_prim_quad_texture_3d(GSGLOBAL *gsGlobal, GSTEXTURE *Texture,
				float x1, float y1, int iz1, float u1, float v1,
				float x2, float y2, int iz2, float u2, float v2,
				float x3, float y3, int iz3, float u3, float v3,
				float x4, float y4, int iz4, float u4, float v4, u64 color)
{
	gsKit_set_texfilter(gsGlobal, Texture->Filter);

	u64* p_store;
	u64* p_data;
	int qsize = 6;
	int bsize = 96;

	int tw = 31 - (lzw(Texture->Width) + 1);
	if(Texture->Width > (1<<tw))
		tw++;

	int th = 31 - (lzw(Texture->Height) + 1);
	if(Texture->Height > (1<<th))
		th++;

	int ix1 = (int)(x1 * 16.0f) + gsGlobal->OffsetX;
	int ix2 = (int)(x2 * 16.0f) + gsGlobal->OffsetX;
	int ix3 = (int)(x3 * 16.0f) + gsGlobal->OffsetX;
	int ix4 = (int)(x4 * 16.0f) + gsGlobal->OffsetX;

	int iy1 = (int)(y1 * 16.0f) + gsGlobal->OffsetY;
	int iy2 = (int)(y2 * 16.0f) + gsGlobal->OffsetY;
	int iy3 = (int)(y3 * 16.0f) + gsGlobal->OffsetY;
	int iy4 = (int)(y4 * 16.0f) + gsGlobal->OffsetY;

	int iu1 = (int)(u1 * 16.0f);
	int iu2 = (int)(u2 * 16.0f);
	int iu3 = (int)(u3 * 16.0f);
	int iu4 = (int)(u4 * 16.0f);

	int iv1 = (int)(v1 * 16.0f);
	int iv2 = (int)(v2 * 16.0f);
	int iv3 = (int)(v3 * 16.0f);
	int iv4 = (int)(v4 * 16.0f);

	p_store = p_data = gsKit_heap_alloc(gsGlobal, qsize, bsize, GIF_PRIM_QUAD_TEXTURED);

	*p_data++ = GIF_TAG_QUAD_TEXTURED(0);
	*p_data++ = GIF_TAG_QUAD_TEXTURED_REGS(gsGlobal->PrimContext);

	if(Texture->VramClut == 0)
	{
		*p_data++ = GS_SETREG_TEX0(Texture->Vram/256, Texture->TBW, Texture->PSM,
			tw, th, gsGlobal->PrimAlphaEnable, 0,
			0, 0, Texture->ClutPSM, 0, GS_CLUT_STOREMODE_NOLOAD);
	}
	else
	{
		*p_data++ = GS_SETREG_TEX0(Texture->Vram/256, Texture->TBW, Texture->PSM,
			tw, th, gsGlobal->PrimAlphaEnable, 0,
			Texture->VramClut/256, 0, 0, 0, GS_CLUT_STOREMODE_LOAD);
	}

	*p_data++ = GS_SETREG_PRIM( GS_PRIM_PRIM_TRISTRIP, 0, 1, gsGlobal->PrimFogEnable,
				gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable,
				1, gsGlobal->PrimContext, 0);

	*p_data++ = color;

	*p_data++ = GS_SETREG_UV( iu1, iv1 );
	*p_data++ = GS_SETREG_XYZ2( ix1, iy1, iz1 );

	*p_data++ = GS_SETREG_UV( iu2, iv2 );
	*p_data++ = GS_SETREG_XYZ2( ix2, iy2, iz2 );

	*p_data++ = GS_SETREG_UV( iu3, iv3 );
	*p_data++ = GS_SETREG_XYZ2( ix3, iy3, iz3 );

	*p_data++ = GS_SETREG_UV( iu4, iv4 );
	*p_data++ = GS_SETREG_XYZ2( ix4, iy4, iz4 );
}

void gsKit_prim_quad_goraud_texture_3d(GSGLOBAL *gsGlobal, GSTEXTURE *Texture,
				float x1, float y1, int iz1, float u1, float v1,
				float x2, float y2, int iz2, float u2, float v2,
				float x3, float y3, int iz3, float u3, float v3,
				float x4, float y4, int iz4, float u4, float v4,
				u64 color1, u64 color2, u64 color3, u64 color4)
{
	gsKit_set_texfilter(gsGlobal, Texture->Filter);

	u64* p_store;
	u64* p_data;
	int qsize = 7;
	int bsize = 112;

	int tw = 31 - (lzw(Texture->Width) + 1);
	if(Texture->Width > (1<<tw))
		tw++;

	int th = 31 - (lzw(Texture->Height) + 1);
	if(Texture->Height > (1<<th))
		th++;

	int ix1 = (int)(x1 * 16.0f) + gsGlobal->OffsetX;
	int ix2 = (int)(x2 * 16.0f) + gsGlobal->OffsetX;
	int ix3 = (int)(x3 * 16.0f) + gsGlobal->OffsetX;
	int ix4 = (int)(x4 * 16.0f) + gsGlobal->OffsetX;

	int iy1 = (int)(y1 * 16.0f) + gsGlobal->OffsetY;
	int iy2 = (int)(y2 * 16.0f) + gsGlobal->OffsetY;
	int iy3 = (int)(y3 * 16.0f) + gsGlobal->OffsetY;
	int iy4 = (int)(y4 * 16.0f) + gsGlobal->OffsetY;

	int iu1 = (int)(u1 * 16.0f);
	int iu2 = (int)(u2 * 16.0f);
	int iu3 = (int)(u3 * 16.0f);
	int iu4 = (int)(u4 * 16.0f);

	int iv1 = (int)(v1 * 16.0f);
	int iv2 = (int)(v2 * 16.0f);
	int iv3 = (int)(v3 * 16.0f);
	int iv4 = (int)(v4 * 16.0f);

	p_store = p_data = gsKit_heap_alloc(gsGlobal, qsize, bsize, GIF_PRIM_QUAD_TEXTURED);

	*p_data++ = GIF_TAG_QUAD_GORAUD_TEXTURED(0);
	*p_data++ = GIF_TAG_QUAD_GORAUD_TEXTURED_REGS(gsGlobal->PrimContext);

	if(Texture->VramClut == 0)
	{
		*p_data++ = GS_SETREG_TEX0(Texture->Vram/256, Texture->TBW, Texture->PSM,
			tw, th, gsGlobal->PrimAlphaEnable, 0,
			0, 0, Texture->ClutPSM, 0, GS_CLUT_STOREMODE_NOLOAD);
	}
	else
	{
		*p_data++ = GS_SETREG_TEX0(Texture->Vram/256, Texture->TBW, Texture->PSM,
			tw, th, gsGlobal->PrimAlphaEnable, 0,
			Texture->VramClut/256, 0, 0, 0, GS_CLUT_STOREMODE_LOAD);
	}

	*p_data++ = GS_SETREG_PRIM( GS_PRIM_PRIM_TRISTRIP, 1, 1, gsGlobal->PrimFogEnable,
				gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable,
				1, gsGlobal->PrimContext, 0);

	*p_data++ = color1;
	*p_data++ = GS_SETREG_UV( iu1, iv1 );
	*p_data++ = GS_SETREG_XYZ2( ix1, iy1, iz1 );

  *p_data++ = color2;
	*p_data++ = GS_SETREG_UV( iu2, iv2 );
	*p_data++ = GS_SETREG_XYZ2( ix2, iy2, iz2 );

  *p_data++ = color3;
	*p_data++ = GS_SETREG_UV( iu3, iv3 );
	*p_data++ = GS_SETREG_XYZ2( ix3, iy3, iz3 );

  *p_data++ = color4;
	*p_data++ = GS_SETREG_UV( iu4, iv4 );
	*p_data++ = GS_SETREG_XYZ2( ix4, iy4, iz4 );
}
