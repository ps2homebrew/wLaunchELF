#ifndef _HDL_RPC_H
#define _HDL_RPC_H

#define HDL_IRX 0xD0D0D0D

#define HDL_GETINFO 0x004
#define HDL_RENAME 0x005

typedef struct
{
    char Partition_Name[32 + 1];
    char Name[64 + 1];
    char Startup[8 + 1 + 3 + 1];
    int Is_Dvd;
} GameInfo;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

int Hdl_Info_BindRpc(void);
int HdlGetGameInfo(char *PartName, GameInfo *Game);
int HdlRenameGame(char *OldName, char *NewName);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _HDL_RPC_H */
