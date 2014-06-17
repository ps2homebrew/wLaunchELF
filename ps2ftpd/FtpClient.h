#ifndef __FTPCLIENT_H__
#define __FTPCLIENT_H__

/*
 * FtpClient.h - FTP client declaration
 *
 * Copyright (C) 2004 Jesper Svennevid
 *
 * Licensed under the AFL v2.0. See the file LICENSE included with this
 * distribution for licensing terms.
 *
 */

#include "FileSystem.h"

struct FtpServer;

typedef enum
{
	DATAMODE_IDLE,			// no data flow
	DATAMODE_READ,			// reading data from connection
	DATAMODE_WRITE,			// writing data to connection
} DataMode;

typedef enum
{
	CONNSTATE_IDLE,			// connection-state is idle (no pending/active connection)
	CONNSTATE_LISTEN,		// listening for connection
	CONNSTATE_CONNECT,	// pending connection
	CONNSTATE_RUNNING,	// active connection
} ConnState;

typedef enum
{
	AUTHSTATE_INVALID,	// not authenticated
	AUTHSTATE_PASSWORD,	// require password
	AUTHSTATE_FAKEPASS,	// require password, but fail
	AUTHSTATE_VALID,		// authenticated
} AuthState;

typedef enum
{
	DATAACTION_NONE,
	DATAACTION_LIST,
	DATAACTION_NLST,
	DATAACTION_RETR,
	DATAACTION_STOR,
	DATAACTION_APPE,
} DataAction;

#define FCOMMAND(a,b,c,d) (((a)<<24)|((b)<<16)|((c)<<8)|(d))

enum
{
	// required commands for functional ftp

	FTPCMD_USER = FCOMMAND('u','s','e','r'),
	FTPCMD_QUIT = FCOMMAND('q','u','i','t'),
	FTPCMD_PORT = FCOMMAND('p','o','r','t'),
	FTPCMD_TYPE = FCOMMAND('t','y','p','e'),
	FTPCMD_MODE = FCOMMAND('m','o','d','e'),
	FTPCMD_STRU = FCOMMAND('s','t','r','u'),
	FTPCMD_RETR = FCOMMAND('r','e','t','r'),
	FTPCMD_STOR = FCOMMAND('s','t','o','r'),
	FTPCMD_NOOP = FCOMMAND('n','o','o','p'),

	// additional commands

	FTPCMD_PASS = FCOMMAND('p','a','s','s'),
	FTPCMD_CWD  = FCOMMAND(  0,'c','w','d'),
	FTPCMD_XCWD = FCOMMAND('x','c','w','d'),
	FTPCMD_CDUP = FCOMMAND('c','d','u','p'),
	FTPCMD_PASV = FCOMMAND('p','a','s','v'),
	FTPCMD_APPE = FCOMMAND('a','p','p','e'),
	FTPCMD_RNFR = FCOMMAND('r','n','f','r'),
	FTPCMD_RNTO = FCOMMAND('r','n','t','o'),
	FTPCMD_ABOR = FCOMMAND('a','b','o','r'), // implement
	FTPCMD_DELE = FCOMMAND('d','e','l','e'),
	FTPCMD_RMD  = FCOMMAND(  0,'r','m','d'),
	FTPCMD_XRMD = FCOMMAND('x','r','m','d'),
	FTPCMD_MKD  = FCOMMAND(  0,'m','k','d'),
	FTPCMD_XMKD = FCOMMAND('x','m','k','d'),
	FTPCMD_PWD  = FCOMMAND(  0,'p','w','d'),
	FTPCMD_XPWD = FCOMMAND('x','p','w','d'),
	FTPCMD_LIST = FCOMMAND('l','i','s','t'),
	FTPCMD_NLST = FCOMMAND('n','l','s','t'),
	FTPCMD_SITE = FCOMMAND('s','i','t','e'),
	FTPCMD_SYST = FCOMMAND('s','y','s','t'),
	FTPCMD_REST = FCOMMAND('r','e','s','t'),
	FTPCMD_SIZE = FCOMMAND('s','i','z','e'),

	// possible commands, we'll implement them if it become necessary

	//FTPCMD_FEAT = FCOMMAND('f','e','a','t'),
	//FTPCMD_STAT = FCOMMAND('s','t','a','t'),
	//FTPCMD_HELP = FCOMMAND('h','e','l','p'),
};

enum
{
	SITECMD_MNT  = FCOMMAND(  0,'m','n','t'),
	SITECMD_UMNT = FCOMMAND('u','m','n','t'),
	SITECMD_SYNC = FCOMMAND('s','y','n','c'),
	SITECMD_HELP = FCOMMAND('h','e','l','p'),
};

typedef struct FtpClientContainer
{
	struct FtpClient* m_pClient;
	struct FtpClientContainer* m_pPrev;
	struct FtpClientContainer* m_pNext;
} FtpClientContainer;

typedef struct FtpClient
{
	struct FtpServer* m_pServer;	// owning server

	int m_iControlSocket;
	AuthState m_eAuthState;
	int m_iAuthAttempt;

	int m_iDataSocket;
	unsigned int m_uiDataBufferSize;
	int m_iRemoteAddress[4]; // yeah, I know, but FTP is retarded
	int m_iRemotePort;
	DataMode m_eDataMode;
	DataAction m_eDataAction;
	ConnState m_eConnState;
	int m_uiDataOffset;
	int m_iRestartMarker;	// TODO: 64-bit support?

	FtpClientContainer m_kContainer;

	int m_iCommandOffset;
	char m_CommandBuffer[512];

	FSContext m_kContext;

	const char** m_pMessages;
} FtpClient;

void FtpClient_Create( FtpClient* pClient, struct FtpServer* pServer, int iControlSocket );
void FtpClient_Destroy( FtpClient* pClient );
void FtpClient_Send( FtpClient* pClient, int iReturnCode, const char* pString );
void FtpClient_OnConnect( FtpClient* pClient );
void FtpClient_OnCommand( FtpClient* pClient, const char* pString );

void FtpClient_OnCmdQuit( FtpClient* pClient );
void FtpClient_OnCmdUser( FtpClient* pClient, const char* pUser );
void FtpClient_OnCmdPass( FtpClient* pClient, const char* pPass );
void FtpClient_OnCmdPasv( FtpClient* pClient );
void FtpClient_OnCmdPort( FtpClient* pClient, int* ip, int port );
void FtpClient_OnCmdSyst( FtpClient* pClient );
void FtpClient_OnCmdList( FtpClient* pClient, const char* pPath, int iNamesOnly );
void FtpClient_OnCmdType( FtpClient* pClient, const char* pType );
void FtpClient_OnCmdRetr( FtpClient* pClient, const char* pFile );
void FtpClient_OnCmdStor( FtpClient* pClient, const char* pFile );
void FtpClient_OnCmdPwd( FtpClient* pClient );
void FtpClient_OnCmdCwd( FtpClient* pClient, const char* pPath );
void FtpClient_OnCmdDele( FtpClient* pClient, const char* pFile );
void FtpClient_OnCmdMkd( FtpClient* pClient, const char* pDir );
void FtpClient_OnCmdRmd( FtpClient* pClient, const char* pDir );
void FtpClient_OnCmdRnfr( FtpClient* pClient, const char* name );
void FtpClient_OnCmdRnto( FtpClient* pClient, const char* newName );
void FtpClient_OnCmdSite( FtpClient* pClient, const char* pCmd );
void FtpClient_OnCmdMode( FtpClient* pClient, const char* pMode );
void FtpClient_OnCmdStru( FtpClient* pClient, const char* pStructure );
void FtpClient_OnCmdAppe( FtpClient* pClient, const char* pFile );
void FtpClient_OnCmdRest( FtpClient* pClient, int iMarker );
void FtpClient_OnCmdSize( FtpClient* pClient, const char* pFile );
void FtpClient_OnCmdFeat( FtpClient* pClient );

void FtpClient_OnSiteMount( FtpClient* pClient, const char* pMountPoint, const char* pMountFile );
void FtpClient_OnSiteUmount( FtpClient* pClient, const char* pMountPoint );
void FtpClient_OnSiteSync( FtpClient* pClient, const char* pDeviceName );
void FtpClient_OnDataConnect( FtpClient* pClient,  int* ip, int port );
void FtpClient_OnDataConnected( FtpClient* pClient );
void FtpClient_OnDataRead( FtpClient* pClient );
void FtpClient_OnDataWrite( FtpClient* pClient );
void FtpClient_OnDataComplete( FtpClient* pClient, int iReturnCode, const char* pMessage );
void FtpClient_OnDataFailed( FtpClient* pClient, const char* pMessage  );
void FtpClient_OnDataCleanup( FtpClient* pClient );
void FtpClient_HandleDataConnect( FtpClient* pClient );

#endif
