/*
 * FtpServer.c - FTP server methods
 *
 * Copyright (C) 2004 Jesper Svennevid
 *
 * Licensed under the AFL v2.0. See the file LICENSE included with this
 * distribution for licensing terms.
 *
 */


#ifndef LINUX
#include "irx_imports.h"
#define assert(x)
#define VALID_IRX
#else
#include <assert.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#define disconnect(s) close(s)
#endif

#include "FtpServer.h"

#include <errno.h>

void FtpServer_Create( FtpServer* pServer )
{
	int i;

	assert( pServer );

	pServer->m_iSocket = -1;
	pServer->m_iPort = 21;

	pServer->m_kClients.m_pClient = NULL;
	pServer->m_kClients.m_pPrev = &(pServer->m_kClients);
	pServer->m_kClients.m_pNext = &(pServer->m_kClients);

	pServer->m_iAnonymous = 0;
	strcpy( pServer->m_Username, "ps2dev" );
	strcpy( pServer->m_Password, "ps2dev" );

	for( i = 0; i < MAX_CLIENTS; i++ )
		pServer->m_kClientArray[i].m_pServer = NULL;
}

void FtpServer_Destroy( FtpServer* pServer )
{
	assert( pServer );

	FtpServer_Stop(pServer);
}


int FtpServer_Start( FtpServer* pServer )
{
	int opt;
	int s;
	struct sockaddr_in sa;

	assert( pServer );

	// create server socket

	if( (s = socket( PF_INET, SOCK_STREAM, 0 )) < 0 )
	{
		printf("ps2ftpd: failed to create socket.\n");
		return -1;
	}

	// make a few settings

	opt = 1;	
	if( setsockopt( s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt) ) < 0 )
	{
		disconnect(s);
		printf("ps2ftpd: Could not change socket options.\n");
		return -1;
	}

	// try to bind socket

	memset( &sa, 0, sizeof(sa) );
	sa.sin_family = AF_INET;
	sa.sin_port = htons(pServer->m_iPort&0x7fffffff);
	sa.sin_addr.s_addr = INADDR_ANY;

	if( bind( s, (struct sockaddr*)&sa, sizeof(sa) ) < 0 )
	{
		printf("ps2ftpd: could not bind to port.\n");
		disconnect(s);
		return -1;
	}

	// listen to socket
	
	if( listen( s, 1 ) < 0 )
	{
		printf("ps2ftpd: could not listen to port.\n");
		disconnect(s);
		return -1;
	}

	printf("ps2ftpd: listening to port %d\n",pServer->m_iPort&0xffff);

	pServer->m_iSocket = s;

	return s;
}

void FtpServer_Stop( FtpServer* pServer )
{
	// disconnect all clients

	while( pServer->m_kClients.m_pNext != &(pServer->m_kClients) )
		FtpServer_OnClientDisconnect( pServer, pServer->m_kClients.m_pNext->m_pClient );

	// close server socket

	if( -1 != pServer->m_iSocket )
	{
		disconnect( pServer->m_iSocket );
		pServer->m_iSocket = -1;
	}
}

void FtpServer_SetPort( FtpServer* pServer, int iPort )
{
	pServer->m_iPort = iPort;
}

void FtpServer_SetAnonymous( FtpServer* pServer, int iAnonymous )
{
	pServer->m_iAnonymous = iAnonymous;
}

void FtpServer_SetUsername( FtpServer* pServer, char* pUsername )
{
	strncpy( pServer->m_Username, pUsername, sizeof(pServer->m_Username) );
	pServer->m_Username[sizeof(pServer->m_Username)-1] = '\0';
}

void FtpServer_SetPassword( FtpServer* pServer, char* pPassword )
{
	strncpy( pServer->m_Password, pPassword, sizeof(pServer->m_Password) );
	pServer->m_Password[sizeof(pServer->m_Password)-1] = '\0';
}

int FtpServer_IsRunning( FtpServer* pServer )
{
	return ( -1 != pServer->m_iSocket );
}

int FtpServer_HandleEvents( FtpServer* pServer )
{
	int res;
	fd_set r;
	fd_set w;
	int hs = pServer->m_iSocket;

	FD_ZERO( &r );
	FD_ZERO( &w );

	if( -1 != pServer->m_iSocket )
		FD_SET( pServer->m_iSocket, &r );

	// register clients

	FtpServer_ClientTraverse(pServer)
	{
		FD_SET( pClient->m_iControlSocket, &r );

		if( pClient->m_iControlSocket > hs )
			hs = pClient->m_iControlSocket;

		if( -1 != pClient->m_iDataSocket )
		{
			if(( CONNSTATE_CONNECT == pClient->m_eConnState ) || ( DATAMODE_WRITE == pClient->m_eDataMode ))
			{
				FD_SET( pClient->m_iDataSocket, &w );
			}
			else if(( CONNSTATE_LISTEN == pClient->m_eConnState ) || ( DATAMODE_READ == pClient->m_eDataMode ))
			{
				FD_SET( pClient->m_iDataSocket, &r );
			}

			if( pClient->m_iDataSocket > hs )
				hs = pClient->m_iDataSocket;
		}
	}
	FtpServer_ClientEndTraverse;

	res = select( hs+1, &r, &w, NULL, NULL );

	// process server

	if( (-1 != pServer->m_iSocket) && FD_ISSET(pServer->m_iSocket,&r))
	{
		struct sockaddr_in sa;
		socklen_t sl;
		int s;

		sl = sizeof(sa);
		if( (s = accept( pServer->m_iSocket, (struct sockaddr*)&sa,&sl)) >= 0 )
		{
			FtpClient* pClient;
			unsigned int addr;
			unsigned short port;

			addr = htonl(sa.sin_addr.s_addr);
			port = htons(sa.sin_port);

#ifdef DEBUG
			printf( "ps2ftpd: new client session (%d.%d.%d.%d:%d)\n",
												(addr>>24)&0xff, (addr>>16)&0xff, (addr>>8)&0xff, addr&0xff,
												port );
#endif
			pClient = FtpServer_OnClientConnect( pServer, s );
		}
	}

	// process clients

	FtpServer_ClientTraverse(pServer)
	{
		// data is available to read on the controlsocket
		// TODO: handle OOB data for transfer breaks
		if(FD_ISSET(pClient->m_iControlSocket,&r))
		{
			int rv = recv(pClient->m_iControlSocket,
											pClient->m_CommandBuffer+pClient->m_iCommandOffset,
											sizeof(pClient->m_CommandBuffer)-pClient->m_iCommandOffset, 0 );

			if( rv > 0 )
			{
				char *rofs; 
				char *nofs;

				pClient->m_CommandBuffer[pClient->m_iCommandOffset+rv] = '\0';

				rofs = strchr(pClient->m_CommandBuffer+pClient->m_iCommandOffset,'\r');
				nofs = strchr(pClient->m_CommandBuffer+pClient->m_iCommandOffset,'\n');

				pClient->m_iCommandOffset += rv;

				if( nofs )
				{
					int cmdlen = rofs ? (rofs-pClient->m_CommandBuffer) : (nofs-pClient->m_CommandBuffer);

					// extract commandline

					pClient->m_CommandBuffer[cmdlen] = '\0';

					// execute command

#ifdef DEBUG
					printf("%08x << %s\r\n",(unsigned int)pClient,pClient->m_CommandBuffer);
#endif
					FtpClient_OnCommand(pClient,pClient->m_CommandBuffer);

					// get remaining data (safe even if client has disconnected, as it is never freed)

					memcpy( pClient->m_CommandBuffer, nofs+1, sizeof(pClient->m_CommandBuffer)-((nofs+1)-pClient->m_CommandBuffer) );
					pClient->m_iCommandOffset -= (nofs+1)-pClient->m_CommandBuffer;
				}
			}
			else
				FtpServer_OnClientDisconnect(pServer,pClient); // :FIXME: use a preallocated ringbuffer in the server shared between clients?
		}

		// handle client data transfers

		if( -1 != pClient->m_iDataSocket )
		{
			if( ( CONNSTATE_CONNECT == pClient->m_eConnState ) || ( DATAMODE_WRITE == pClient->m_eDataMode ) )
			{
				if(FD_ISSET(pClient->m_iDataSocket,&w))
				{
					if( CONNSTATE_CONNECT == pClient->m_eConnState )
					{
						// something has happened with connecting socket

						unsigned int err;
						socklen_t sl = sizeof(err);

						if( !getsockopt( pClient->m_iDataSocket, SOL_SOCKET, SO_ERROR, &err, &sl ) )
						{
							if( !err )
								FtpClient_OnDataConnected(pClient);
							else
								FtpClient_OnDataFailed(pClient,NULL);
						}
						else
							FtpClient_OnDataFailed(pClient,NULL);
					}
					else
						FtpClient_OnDataWrite(pClient);
				}
			}
			else if( (CONNSTATE_LISTEN == pClient->m_eConnState) || DATAMODE_READ == pClient->m_eDataMode )
			{
				if(FD_ISSET(pClient->m_iDataSocket,&r))
				{
					if( CONNSTATE_LISTEN == pClient->m_eConnState )
					{
						struct sockaddr_in sa;
						socklen_t sl;
						int s;

						sl = sizeof(sa);
						if( (s = accept( pClient->m_iDataSocket, (struct sockaddr*)&sa,&sl)) >= 0 )
						{
							disconnect(pClient->m_iDataSocket);
							pClient->m_iDataSocket = s;
							FtpClient_OnDataConnected(pClient);
						}
					}
					else
						FtpClient_OnDataRead(pClient);
				}
			}
		}
	}
	FtpServer_ClientEndTraverse;

	return res;
}

FtpClient* FtpServer_OnClientConnect( FtpServer* pServer, int iSocket )
{
	FtpClient* pClient;
	int i;

	assert( pServer );

	pClient = NULL;
	for( i = 0; i < MAX_CLIENTS; i++ )
	{
		if( !pServer->m_kClientArray[i].m_pServer )
		{
			pClient = &pServer->m_kClientArray[i];
			break;
		}
	}

	if(!pClient)
	{
		char* buf = "421 Service Unavailable.\r\n";

		send(iSocket,buf,strlen(buf),0);
		disconnect(iSocket);
#ifdef DEBUG
		printf("ps2ftpd: refused connection to client\n");
#endif
		return NULL;
	}

	FtpClient_Create( pClient, pServer, iSocket );

	// attach to server list

	pClient->m_kContainer.m_pPrev = pServer->m_kClients.m_pPrev;
	pClient->m_kContainer.m_pNext = &pServer->m_kClients;

	pServer->m_kClients.m_pPrev->m_pNext = &pClient->m_kContainer;
	pServer->m_kClients.m_pPrev = &pClient->m_kContainer;

	// start session

	FtpClient_OnConnect(pClient);

	return pClient;
}

void FtpServer_OnClientDisconnect( FtpServer* pServer, FtpClient* pClient )
{
	assert( pServer && pClient );

#ifdef DEBUG
	printf("ps2ftpd: disconnected client %08x\n",(unsigned int)pClient);
#endif

	// detach from list

	pClient->m_kContainer.m_pPrev->m_pNext = pClient->m_kContainer.m_pNext;
	pClient->m_kContainer.m_pNext->m_pPrev = pClient->m_kContainer.m_pPrev;

	pClient->m_kContainer.m_pPrev = &pClient->m_kContainer;
	pClient->m_kContainer.m_pNext = &pClient->m_kContainer;

	// destroy client

	FtpClient_Destroy(pClient);
}
