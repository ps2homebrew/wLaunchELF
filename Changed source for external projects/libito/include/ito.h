// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
//
// ito - free library for PlayStation 2 by Jules - www.mouthshut.net
//
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

// 

#ifndef _ITO_H
#define _ITO_H

#ifdef __cplusplus 
	extern "C" {
#endif

#include <itotypes.h>
#include <itogs.h>
#include <itodma.h>
#include <itosyscalls.h>
#include <itopacket.h>
#include <itoimage.h>
#include <itomisc.h>

// Ito version

#define itoVersionInt 021
#define itoVersionStr "0.2.1"

// Ito Functions
void itoInit();

#ifdef __cplusplus 
}
#endif /* __cplusplus */

#endif /* _ITO_H */
