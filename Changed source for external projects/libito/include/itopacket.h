// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
//
// ito - free library for PlayStation 2 by Jules - www.mouthshut.net
//
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

#ifndef _ITOPACKET_H
#define _ITOPACKET_H

#ifdef __cplusplus 
	extern "C" {
#endif /* __cplusplus */

#include <itotypes.h>

#define PACKET_MAX_SIZE	  0x80000
#define PACKET_MAX_SIZE_Q 0x8000

// This value defines how many quads max can be left in a packet before its
// sent and therefor it has to be bigger then any prim is in quads.
#define PACKET_MAX_SIZE_LEFT_Q	40

// PACKET SYSTEM
void itoSetActivePacket(uint64* packet);
uint8 itoSendActivePacket();
uint32 itoGetActivePacketSizeQ();

// GS DOUBLE BUFFER PACKET SYSTEM
void itoCheckGsPacket();
void itoGsFinish();
void itoSendGsPacket();
void itoInitGsPacket();
void itoResetGsPacket();
uint32 itoGetActiveGsPacketSizeQ();

#ifdef __cplusplus 
}
#endif /* __cplusplus */

#endif /* _ITOPACKET_H */
