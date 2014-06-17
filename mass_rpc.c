/*
 * mass_rpc.c - USB Mass storage driver for PS2
 *
 * (C) 2004, Marek Olejnik (ole00@post.cz)
 *
 * functions for communication with the irx
 *
 * See the file LICENSE included with this distribution for licensing terms.
 */

#include <kernel.h>
#include <sifrpc.h>
#include "mass_rpc.h"

#define	MASS_USB_ID	0x500C0F1


static SifRpcClientData_t client __attribute__((aligned(64)));
static unsigned char rpcBuffer[1024] __attribute__((aligned(64)));

/*
void dumpBuffer() {
	int i;
	printf("ee buf: ");
	for (i = 0; i < 20; i++) {
		printf("%02X ", rpcBuffer[i]);
	}
	printf("\n");
}

*/
int usb_mass_bindRpc() {                           
	int ret;
	int retryCount = 0x1000;

	while(retryCount--) {
	        ret = SifBindRpc( &client, MASS_USB_ID, 0);
        	if ( ret  < 0)  {
	           return -1;
	        }
	        if (client.server != 0) break;

	        // short delay 
	      	ret = 0x10000;
	    	while(ret--) asm("nop\nnop\nnop\nnop");
	}
	return retryCount;
}

int usb_mass_getFirstDirentry(char* dirName, fat_dir_record* record) {
        int i;
        int* ret;

        for (i = 0; dirName[i] != 0; i++) {
		rpcBuffer[i+4] = dirName[i];
        }
        rpcBuffer[i+4] = 0;
		
	SifCallRpc(&client,1,0,(void*)(&rpcBuffer[0]),1024,(void*)(&rpcBuffer[0]),1024,0,0);

	ret = (int*) rpcBuffer;
	if (*ret > 0) {
		memcpy(record, rpcBuffer + 4, sizeof(fat_dir_record));
	}
	return *ret;
}

int usb_mass_getNextDirentry(fat_dir_record* record) {
        int* ret;

	SifCallRpc(&client,2,0,(void*)(&rpcBuffer[0]),1024,(void*)(&rpcBuffer[0]),1024,0,0);

	ret = (int*) rpcBuffer;
	if (*ret > 0) {
		memcpy(record, rpcBuffer + 4, sizeof(fat_dir_record));
	}
	return *ret;
}

int usb_mass_dumpSystemInfo() {
        int* ret;

	SifCallRpc(&client,4,0,(void*)(&rpcBuffer[0]), 0,(void*)(&rpcBuffer[0]),256,0,0);

	ret = (int*) rpcBuffer;
	return *ret;
}

int usb_mass_dumpDiskContent(unsigned int startSector, unsigned int sectorCount, char* fname) {
        int* ret;
        unsigned int * nums;
        int i;

	for (i = 0; fname[i] != 0; i++) {
		rpcBuffer[i+8] = fname[i];
        }
        rpcBuffer[i+8] = 0;

       	nums = (unsigned int*) rpcBuffer;
       	nums[0] = startSector;
       	nums[1] = sectorCount;

	SifCallRpc(&client,3,0,(void*)(&rpcBuffer[0]), 256,(void*)(&rpcBuffer[0]),256,0,0);

	ret = (int*) rpcBuffer;
	return *ret;
}

