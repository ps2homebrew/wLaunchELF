// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
//
// ito - free library for PlayStation 2 by Jules - www.mouthshut.net
//
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

#ifndef _ITODMA_H
#define _ITODMA_H

#ifdef __cplusplus 
	extern "C" {
#endif

#include <itotypes.h>
/*
int32 ITO_DMA_BASE[10] = {	0x10008000, 0x10009000,
							0x1000A000, 0x1000B000,
							0x1000B400, 0x1000C000,
							0x1000C400, 0x1000C800,
							0x1000D000, 0x1000D400 };
*/

#define	ITO_DMA_VIF0		0
#define ITO_DMA_VIF1		1
#define ITO_DMA_GIF			2
#define	ITO_DMA_FROM_IPU	3
#define	ITO_DMA_TO_IPU		4
#define	ITO_DMA_SIF0		5
#define	ITO_DMA_SIF1		6
#define	ITO_DMA_SIF2		7
#define	ITO_DMA_FROM_SPR	8
#define	ITO_DMA_TO_SPR		9




#define ITO_DMA_CHCR	0x00
#define	ITO_DMA_MADR	0x10
#define ITO_DMA_QWC		0x20
#define	ITO_DMA_TADR	0x30
#define ITO_DMA_ASR0	0x40
#define	ITO_DMA_ASR1	0x50



// DMA CHANNEL 2 (EE-GS) Registers
#define D2_CHCR      	*((volatile unsigned long*)0x1000A000)
#define D2_MADR      	*((volatile unsigned long*)0x1000A010)
#define D2_QWC      	*((volatile unsigned long*)0x1000A020)
#define D2_TADR      	*((volatile unsigned long*)0x1000A030)
#define D2_ASR0      	*((volatile unsigned long*)0x1000A040)
#define D2_ASR1      	*((volatile unsigned long*)0x1000A050)

#define D5_CHCR      	*((volatile unsigned long*)0x1000C000)
#define D5_MADR      	*((volatile unsigned long*)0x1000C010)
#define D5_QWC      	*((volatile unsigned long*)0x1000C020)

#define D2_DIR_EE_GS	0x101
#define D2_DIR_GS_EE	0x100

// dma channel defines
// CHCR
#define CHCR_STR		0x100

// DMAC (Controller, information from ps2.map / ps2dis 
#define D_CTRL      	*((volatile unsigned long*)0x1000E000)
#define D_STAT      	*((volatile unsigned long*)0x1000E010)
#define D_PCR	      	*((volatile unsigned long*)0x1000E020)
#define D_SQWC      	*((volatile unsigned long*)0x1000E030)
#define D_RBSR      	*((volatile unsigned long*)0x1000E040)
#define D_RBOR      	*((volatile unsigned long*)0x1000E050)
#define D_STADR      	*((volatile unsigned long*)0x1000E060)


void itoDmaCReset();
void itoDma2Wait();
void itoDma2Reset();
void itoDma2Send(void *src, uint32 packets, uint64 direction);


// directions for user
#define EE2GS	0
#define GS2EE	1

#define NONE2NONE 0xFF

// GENERAL DMA FOR ALL SUPPORTED DMA DIRECTIONS
void itoSetDmaDirection(uint8 dir);
void itoSetDmaTDirection(uint8 dir);
void itoRestoreDmaDirection();
uint8 itoGetDmaDirection();
void itoDmaWait();
void itoDmaSend(void* src, uint32 packets);

// DMA TAG

#define DMA_TAG_ID_REF	3
#define DMA_TAG_ID_END	7

#define DMA_TAG_SPR_MEM	0
#define DMA_TAG_SPR_SPR	1

void itoDmaTag(uint16 qwc, uint8 pce, uint8 id, uint8 irq, void *addr, uint8 spr);


/// --- 

void itoD2_TADR(uint32 addr, uint8 spr);
void itoD2_MADR(uint32 addr, uint8 spr);
void itoD2_QWC(uint16 size);
void itoD2_CHCR(uint8 dir, uint8 mod, uint8 asp, uint8 tte, uint8 tie, uint8 str, uint16 tag);

void itoDma2TagSend(void *src);

#ifdef __cplusplus 
}
#endif /* __cplusplus */

#endif /* _ITODMA_H */
