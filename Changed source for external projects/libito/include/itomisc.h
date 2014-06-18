// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
//
// ito - free library for PlayStation 2 by Jules - www.mouthshut.net
//
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

#ifndef _ITOMISC_H
#define _ITOMISC_H

#ifdef __cplusplus 
	extern "C" {
#endif

#include <itotypes.h>

uint32 fmti(float f);

#ifndef bzero
#define bzero( _str, _n )            memset2( _str, '\0', _n )
#endif

#ifndef memset
void * memset2(void *block, uint8 c, uint64 size);
#endif

#ifdef __cplusplus 
}
#endif /* __cplusplus */

#endif /* _ITOIMAGE_H */
