#ifndef __FTPSERVER_H__
#define __FTPSERVER_H__

/*
 * FtpServer.h - FTP server declaration
 *
 * Copyright (C) 2004 Jesper Svennevid
 *
 * Licensed under the AFL v2.0. See the file LICENSE included with this
 * distribution for licensing terms.
 *
 */

#include "FtpClient.h"

#define MAX_CLIENTS 4

typedef struct FtpServer
{
	int m_iSocket;
	int m_iPort;

	int m_iAnonymous; // != 0 if anonymous logins are allowed
	char m_Username[32];
	char m_Password[32];

	// double-linked list of connected clients
	FtpClientContainer m_kClients;

	// array to pull clients from
	FtpClient m_kClientArray[MAX_CLIENTS];
} FtpServer;

// NOTE: when using these macros and removing a client,
// it might happen that the new head of the list is never processed.
// I have not worked out how to solve this yet.

#define FtpServer_ClientTraverse(server) \
	{ \
		FtpClientContainer* pClientNext; \
		FtpClientContainer* pClientContainer = (server)->m_kClients.m_pNext; \
		while( pClientContainer != &((server)->m_kClients) ) \
		{ \
			FtpClient* pClient = pClientContainer->m_pClient; \
			pClientNext = pClientContainer->m_pNext;

#define FtpServer_ClientEndTraverse \
			pClientContainer = pClientNext; \
		} \
	}

void FtpServer_Create( FtpServer* pServer );
void FtpServer_Destroy( FtpServer* pServer );
int FtpServer_Start( struct FtpServer* pServer );
void FtpServer_Stop( struct FtpServer* pServer );

void FtpServer_SetPort( FtpServer* pServer, int iPort );
void FtpServer_SetAnonymous( FtpServer* pServer, int iAnonymous );
void FtpServer_SetUsername( FtpServer* pServer, char* pUsername );
void FtpServer_SetPassword( FtpServer* pServer, char* pPassword );

int FtpServer_IsRunning( struct FtpServer* pServer );
int FtpServer_HandleEvents( struct FtpServer* pServer );
FtpClient* FtpServer_OnClientConnect( struct FtpServer* pServer, int iSocket );
void FtpServer_OnClientDisconnect( struct FtpServer* pServer, FtpClient* pClient );

#endif
