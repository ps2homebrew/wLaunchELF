// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
//
// ito - free library for PlayStation 2 by Jules - www.mouthshut.net
//
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

#ifndef _ITOIMAGE_H
#define _ITOIMAGE_H

#ifdef __cplusplus 
	extern "C" {
#endif

#include <itotypes.h>

#define ITO_CLUT8_RGBA32	3
#define ITO_CLUT8_RGBA16	4
#define ITO_CLUT4_RGBA32	5
#define ITO_CLUT4_RGBA16	6

typedef struct{
	uint32 identifier; // 'IIF1' - IIFv (v = version of iif)
	uint32 width;
	uint32 height;
	uint32 psm;
} hIIF;


hIIF itoGetIIFHeader(void *src);
void itoLoadIIF(void *src, uint32 dest, uint32 tbw, uint16 x, uint16 y);
void itoLoadIFFClut(void *src, uint32 dest, uint16 cbw, uint16 x, uint16 y);
uint32 itoGetIIFSize(void* iif);
uint8 itoGetIIFClutType(uint8 cpsm);
uint8 itoGetIIFPsmType(uint8 cpsm);
uint32 itoGetIIFClutSize(uint8 cpsm);

#ifdef __cplusplus 
}
#endif /* __cplusplus */

#endif /* _ITOIMAGE_H */
