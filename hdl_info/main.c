#include <thbase.h>
#include <thevent.h>
#include <iomanX.h>
#include <stdio.h>
#include <loadcore.h>
#include <intrman.h>
#include <sys/stat.h>
#include <dev9.h>
#include <sifrpc.h>
#include <sysmem.h>

#include "main.h"
#include "ps2_hdd.h"
#include "hdd.h"

int __attribute__((unused)) shutdown() { return 0; }

/* function declaration */
void rpcMainThread(void *param);
void *rpcCommandHandler(int command, void *Data, int Size);

static SifRpcDataQueue_t Rpc_Queue __attribute__((aligned(64)));
static SifRpcServerData_t Rpc_Server __attribute((aligned(64)));
static int Rpc_Buffer[1024] __attribute((aligned(64)));

/* Description: Module entry point */
int _start(int argc, char **argv)
{
    iop_thread_t param;
    int id;

    printf("Hdl Info: PS2 HDLoader Information Module v 0.1\n");
    printf("Hdl Info: 2006 Polo\n");

    printf("Hdl Info: IOP RPC Initialization.\n");
    /*create thread*/
    param.attr = TH_C;
    param.thread = rpcMainThread;
    param.priority = 40;
    param.stacksize = 0x800;
    param.option = 0;

    id = CreateThread(&param);
    if (id > 0) {
        StartThread(id, 0);
        return 0;
    } else
        return 1;

    return MODULE_RESIDENT_END;
}

void rpcMainThread(void *param)
{
    SifInitRpc(0);
    SifSetRpcQueue(&Rpc_Queue, GetThreadId());
    SifRegisterRpc(&Rpc_Server, HDL_IRX, (void *)rpcCommandHandler, (u8 *)&Rpc_Buffer, 0, 0, &Rpc_Queue);
    SifRpcLoop(&Rpc_Queue);
}

void *rpcCommandHandler(int command, void *Data, int Size)
{
    switch (command) {
        case 4:  //HDL Get Game Info
            ((int *)Data)[0] = HdlGetGameInfo((char *)Data, (GameInfo *)(Data + 4));
            break;
        case 5:  //HDL Rename Game
            ((int *)Data)[0] = HdlRenameGame((char *)Data);
            break;
        default:
            break;
    }
    return Data;
}

void *malloc(int size)
{
    int OldState;
    void *result;

    CpuSuspendIntr(&OldState);
    result = AllocSysMemory(ALLOC_FIRST, size, NULL);
    CpuResumeIntr(OldState);

    return result;
}

void free(void *ptr)
{
    int OldState;

    CpuSuspendIntr(&OldState);
    FreeSysMemory(ptr);
    CpuResumeIntr(OldState);
}

