/*      
  _____     ___ ____ 
   ____|   |    ____|      PS2 Open Source Project
  |     ___|   |____       
  
---------------------------------------------------------------------------
*/

#include <cdvdman.h>
#include <intrman.h>
#include <loadcore.h>
#include <sifcmd.h>
#include <sysclib.h>
#include <sysmem.h>
#include <thbase.h>
#include <stdio.h> 

#define MODNAME "chkesr"
IRX_ID(MODNAME, 1, 1);

#define CHKESR_IRX 0x0E0E0E0

int __attribute__((unused)) shutdown() { return 0; }

/* function declaration */
void rpcMainThread(void* param);
void *rpcCommandHandler(int command, void *Data, int Size);
void *Check_ESR_Disc(void *Data);
void *SysAlloc(u64 size);
int   SysFree(void *area);

static SifRpcDataQueue_t Rpc_Queue __attribute__((aligned(64)));
static SifRpcServerData_t Rpc_Server __attribute((aligned(64)));
static int Rpc_Buffer[1024] __attribute((aligned(64)));

//--------------------------------------------------------------
/* Description: Module entry point */
int _start(int argc, char **argv)
{
 iop_thread_t param;
 int id;

 /*create thread*/
 param.attr      = TH_C;
 param.thread    = rpcMainThread;
 param.priority  = 40;
 param.stacksize = 0x800;
 param.option    = 0;

 if((id = CreateThread(&param)) <= 0)
  return MODULE_NO_RESIDENT_END;

 StartThread(id,0);

 return MODULE_RESIDENT_END;
}
//--------------------------------------------------------------
void rpcMainThread(void* param)
{
 SifInitRpc(0);
 SifSetRpcQueue(&Rpc_Queue, GetThreadId());
 SifRegisterRpc(&Rpc_Server, CHKESR_IRX, (void *) rpcCommandHandler, (u8 *) &Rpc_Buffer, 0, 0, &Rpc_Queue);
 SifRpcLoop(&Rpc_Queue);
}
//--------------------------------------------------------------
void *rpcCommandHandler(int command, void *Data, int Size)
{
	switch(command)
	{
		case 1:
			Data = Check_ESR_Disc(Data);
			break;
	}

	return Data;
}
//--------------------------------------------------------------
void *Check_ESR_Disc(void *Data)
{
	
 u32 *ret = Data;
 cd_read_mode_t s_DVDReadMode;
 u8 *buf = NULL;
 int offs = 12;
 int i; 
 int r = 0;
 
 //printf("Check_ESR_Disc starting.\n");
 
 *ret = -1;

 buf = (u8*)SysAlloc((2048 + 0xFF) & ~(u32)0xFF);
 if (buf != NULL) *ret = 0;
 
 if (*ret == 0) {
 	s_DVDReadMode.trycount = 5;
 	s_DVDReadMode.spindlctrl = CdSpinStm;
 	s_DVDReadMode.datapattern = CdSecS2048;
 	s_DVDReadMode.pad = 0x00;

 	for (i=0; i<2048; i++) buf[i] = 0;
 	 	
	//sceCdDiskReady(0);
	//r = sceCdRead(14, 1, buf, &s_DVDReadMode); // read LBA 14
	r = sceCdReadDVDV(14, 1, buf, &s_DVDReadMode); // read LBA 14
	sceCdSync(0);
	
 	if (r <= 0) *ret = -1;
 	else if (!strncmp(buf + offs + 25, "+NSR", 4))
 	     	*ret = 1;

 	SysFree(buf); 	
 }	
 
 //printf("Check_ESR_Disc returning %ld\n", *ret);
 return Data; 
}
//--------------------------------------------------------------
void *SysAlloc(u64 size)
{
 int oldstate;
 register void *p;
 
 CpuSuspendIntr(&oldstate);
 p = AllocSysMemory(ALLOC_FIRST, size, NULL);
 CpuResumeIntr(oldstate);

 return p;
}
//--------------------------------------------------------------
int SysFree(void *area)
{
 int oldstate;
 register int r;

 CpuSuspendIntr(&oldstate);
 r = FreeSysMemory(area);
 CpuResumeIntr(oldstate);

 return r;
}
//--------------------------------------------------------------
