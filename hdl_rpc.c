#include <tamtypes.h>
#include <kernel.h>
#include <sifrpc.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#include "launchelf.h"

static SifRpcClientData_t client __attribute__((aligned(64)));
static int Rpc_Buffer[1024] __attribute__((aligned(64)));

typedef struct
{
    char Partition_Name[32 + 1];
} Rpc_Packet_Send_GetInfo;

typedef struct
{
    char OldName[64];
    char NewName[64];
} Rpc_Packet_Send_Rename;

int Hdl_Inited = 0;

int Hdl_Info_BindRpc()
{
    int ret;
    int retryCount = 0x1000;

    while (retryCount--) {
        ret = SifBindRpc(&client, HDL_IRX, 0);
        if (ret < 0) {
            printf("Hdl Info: EE Bind RPC Error.\n");
            return -1;
        }
        if (client.server != 0) {
            printf("Hdl Info: EE Bind RPC Set.\n");
            break;
        }

        // short delay
        ret = 0x10000;
        while (ret--)
            asm("nop\nnop\nnop\nnop");
    }

    Hdl_Inited = 1;
    return retryCount;
}

int HdlGetGameInfo(char *PartName, GameInfo *Game)
{

    Rpc_Packet_Send_GetInfo *Packet = (Rpc_Packet_Send_GetInfo *)Rpc_Buffer;

    if (!Hdl_Inited)
        return -1;

    strcpy(Packet->Partition_Name, PartName);

    SifCallRpc(&client, HDL_GETINFO, 0, (void *)Rpc_Buffer, sizeof(Rpc_Packet_Send_GetInfo), (void *)Rpc_Buffer, sizeof(GameInfo) + 4, 0, 0);

    memcpy(Game, (void *)(((u8 *)Rpc_Buffer) + 4), sizeof(GameInfo));

    return Rpc_Buffer[0];
}

int HdlRenameGame(char *OldName, char *NewName)
{

    Rpc_Packet_Send_Rename *Packet = (Rpc_Packet_Send_Rename *)Rpc_Buffer;

    if (!Hdl_Inited)
        return -1;

    strcpy(Packet->OldName, OldName);
    strcpy(Packet->NewName, NewName);

    SifCallRpc(&client, HDL_RENAME, 0, (void *)(Rpc_Buffer), sizeof(Rpc_Packet_Send_Rename), (void *)Rpc_Buffer, 4, 0, 0);

    return Rpc_Buffer[0];
}
