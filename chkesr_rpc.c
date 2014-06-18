/*      
  _____     ___ ____ 
   ____|   |    ____|      PS2 Open Source Project
  |     ___|   |____       
  
---------------------------------------------------------------------------

    Copyright (C) 2008 - Neme & jimmikaelkael (www.psx-scene.com) 

    This program is free software; you can redistribute it and/or modify
    it under the terms of the Free McBoot License.
    
	This program and any related documentation is provided "as is"
	WITHOUT ANY WARRANTIES, either express or implied, including, but not
 	limited to, implied warranties of fitness for a particular purpose. The
 	entire risk arising out of use or performance of the software remains
 	with you.
   	In no event shall the author be liable for any damages whatsoever
 	(including, without limitation, damages to your hardware or equipment,
 	environmental damage, loss of health, or any kind of pecuniary loss)
 	arising out of the use of or inability to use this software or
 	documentation, even if the author has been advised of the possibility of
 	such damages.

    You should have received a copy of the Free McBoot License along with
    this program; if not, please report at psx-scene :
    http://psx-scene.com/forums/freevast/

---------------------------------------------------------------------------
*/

#include <tamtypes.h>
#include <kernel.h>
#include <sifrpc.h>

// External functions
int chkesr_rpc_Init(void);
int Check_ESR_Disc(void);

#define CHKESR_IRX 0x0E0E0E0

static SifRpcClientData_t chkesr    __attribute__((aligned(64)));
static int Rpc_Buffer[1024] __attribute__((aligned(64)));

typedef struct {
	u32 ret;
} Rpc_Packet_Send_Check_ESR_Disc;

int chkesr_Inited  = 0;

//--------------------------------------------------------------
int chkesrBindRpc(void) {
	int ret;
	int retryCount = 0x1000;

	while(retryCount--) {
	        ret = SifBindRpc( &chkesr, CHKESR_IRX, 0);
        	if ( ret  < 0) return -1;
	        if (chkesr.server != 0) break;
	        // short delay 
	      	ret = 0x10000;
	    	while(ret--) asm("nop\nnop\nnop\nnop");
	}
	chkesr_Inited = 1;
	return retryCount;
}
//--------------------------------------------------------------
int chkesr_rpc_Init(void)
{
 	chkesrBindRpc();
 	if(!chkesr_Inited) return -1;
 	return 1;
}
//--------------------------------------------------------------
int Check_ESR_Disc(void)
{
 	Rpc_Packet_Send_Check_ESR_Disc *Packet = (Rpc_Packet_Send_Check_ESR_Disc *)Rpc_Buffer;
 	
	if(!chkesr_Inited) chkesr_rpc_Init();
   	if ((SifCallRpc(&chkesr, 1, 0, (void*)Rpc_Buffer, sizeof(Rpc_Packet_Send_Check_ESR_Disc), (void*)Rpc_Buffer, sizeof(int),0,0)) < 0) return -1;
 	return Packet->ret;
}
//--------------------------------------------------------------
