// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
//
// ito - free library for PlayStation 2 by Jules - www.mouthshut.net
//
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

#ifndef _ITOTYPES_H
#define _ITOTYPES_H

#ifdef __cplusplus 
	extern "C" {
#endif

#define bit(n)		(1 << n)
#define align(n)	__attribute__ ((aligned(n)))
#define TRUE		1
#define FALSE		0

typedef char int8;
typedef short int16;
typedef int int32;
typedef long int64;

// Taken from ps2lib
//#ifdef _EE
typedef int			int128 __attribute__(( mode(TI) ));
//#endif

typedef	unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
typedef unsigned long uint64;

// Taken from ps2lib
//#ifdef _EE
typedef unsigned int		uint128 __attribute__(( mode(TI) ));
//#endif


typedef uint64 pkt align(128);

#ifdef __cplusplus 
}
#endif /* __cplusplus */

#endif /* _ITOTYPES_H */
