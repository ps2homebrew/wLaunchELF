//---------------------------------------------------------------------------
//File name:   cd.c
//---------------------------------------------------------------------------
#include "launchelf.h"

#define CD_SERVER_INIT			0x80000592
#define CD_SERVER_SCMD			0x80000593
#define CD_SCMD_GETDISCTYPE		0x03

SifRpcClientData_t clientInit __attribute__ ((aligned(64)));
u32 initMode __attribute__ ((aligned(64)));
s32 cdThreadId = 0;
s32 bindSearchFile = -1;
s32 bindDiskReady = -1;
s32 bindInit = -1;
s32 bindNCmd = -1;
s32 bindSCmd = -1;
s32 nCmdSemaId = -1;		// n-cmd semaphore id
s32 sCmdSemaId = -1;		// s-cmd semaphore id
s32 callbackSemaId = -1;	// callback semaphore id
s32 cdDebug = 0;
s32 sCmdNum = 0;
SifRpcClientData_t clientSCmd __attribute__ ((aligned(64)));
u8 sCmdRecvBuff[48] __attribute__ ((aligned(64)));
volatile s32 cbSema = 0;
ee_thread_t cdThreadParam;
s32 callbackThreadId = 0;
volatile s32 cdCallbackNum __attribute__ ((aligned(64)));

void cdSemaInit(void);
s32 cdCheckSCmd(s32 cur_cmd);
s32 cdSyncS(s32 mode);
void cdSemaExit(void);

//---------------------------------------------------------------------------
s32 cdInit(s32 mode)
{
	s32 i;

	if (cdSyncS(1))
		return 0;
	SifInitRpc(0);
	cdThreadId = GetThreadId();
	bindSearchFile = -1;
	bindNCmd = -1;
	bindSCmd = -1;
	bindDiskReady = -1;
	bindInit = -1;

	while (1) {
		if (SifBindRpc(&clientInit, CD_SERVER_INIT, 0) >= 0)
			if (clientInit.server != 0) break;
		i = 0x10000;
		while (i--);
	}

	bindInit = 0;
	initMode = mode;
	SifWriteBackDCache(&initMode, 4);
	if (SifCallRpc(&clientInit, 0, 0, &initMode, 4, 0, 0, 0, 0) < 0)
		return 0;
	if (mode == CDVD_INIT_EXIT) {
		cdSemaExit();
		nCmdSemaId = -1;
		sCmdSemaId = -1;
		callbackSemaId = -1;
	} else {
		cdSemaInit();
	}
	return 1;
}

//---------------------------------------------------------------------------
void cdSemaExit(void)
{
	if (callbackThreadId) {
		cdCallbackNum = -1;
		SignalSema(callbackSemaId);
	}
	DeleteSema(nCmdSemaId);
	DeleteSema(sCmdSemaId);
	DeleteSema(callbackSemaId);
}

//---------------------------------------------------------------------------
void cdSemaInit(void)
{
	struct t_ee_sema semaParam;

	// return if both semaphores are already inited
	if (nCmdSemaId != -1 && sCmdSemaId != -1)
		return;

	semaParam.init_count = 1;
	semaParam.max_count = 1;
	semaParam.option = 0;
	nCmdSemaId = CreateSema(&semaParam);
	sCmdSemaId = CreateSema(&semaParam);

	semaParam.init_count = 0;
	callbackSemaId = CreateSema(&semaParam);

	cbSema = 0;
}

//---------------------------------------------------------------------------
s32 cdCheckSCmd(s32 cur_cmd)
{
	s32 i;
	cdSemaInit();
	if (PollSema(sCmdSemaId) != sCmdSemaId) {
		if (cdDebug > 0)
			printf("Scmd fail sema cur_cmd:%d keep_cmd:%d\n", cur_cmd, sCmdNum);
		return 0;
	}
	sCmdNum = cur_cmd;
	ReferThreadStatus(cdThreadId, &cdThreadParam);
	if (cdSyncS(1)) {
		SignalSema(sCmdSemaId);
		return 0;
	}

	SifInitRpc(0);
	if (bindSCmd >= 0)
		return 1;
	while (1) {
		if (SifBindRpc(&clientSCmd, CD_SERVER_SCMD, 0) < 0) {
			if (cdDebug > 0)
				printf("Libcdvd bind err S cmd\n");
		}
		if (clientSCmd.server != 0)
			break;

		i = 0x10000;
		while (i--)
			;
	}

	bindSCmd = 0;
	return 1;
}

//---------------------------------------------------------------------------
s32 cdSyncS(s32 mode)
{
	if (mode == 0) {
		if (cdDebug > 0)
			printf("S cmd wait\n");
		while (SifCheckStatRpc(&clientSCmd))
			;
		return 0;
	}
	return SifCheckStatRpc(&clientSCmd);
}

//---------------------------------------------------------------------------
CdvdDiscType_t cdGetDiscType(void)
{
	if (cdCheckSCmd(CD_SCMD_GETDISCTYPE) == 0)
		return 0;

	if (SifCallRpc(&clientSCmd, CD_SCMD_GETDISCTYPE, 0, 0, 0, sCmdRecvBuff, 4, 0, 0) < 0) {
		SignalSema(sCmdSemaId);
		return 0;
	}

	SignalSema(sCmdSemaId);
	return *(s32 *) UNCACHED_SEG(sCmdRecvBuff);
}
//------------------------------
//endfunc cdGetDiscType
//---------------------------------------------------------------------------
//End of file: cd.c
//---------------------------------------------------------------------------
