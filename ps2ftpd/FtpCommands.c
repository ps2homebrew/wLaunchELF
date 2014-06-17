/*
 * FtpCommands.c - FTP command handlers
 *
 * Copyright (C) 2004 Jesper Svennevid
 *
 * Licensed under the AFL v2.0. See the file LICENSE included with this
 * distribution for licensing terms.
 *
 */

#include "FtpClient.h"
#include "FtpServer.h"
#include "FtpMessages.h"

#ifndef LINUX
#include "irx_imports.h"
#define assert(x)
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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#define disconnect(s) close(s)
#endif

#include <errno.h>

// present in FtpClient.c
extern char* itoa(char* in, int val);

static char buffer[512];

void FtpClient_OnCmdQuit( FtpClient* pClient )
{
	assert( pClient );

	FtpClient_Send( pClient, 221, pClient->m_pMessages[FTPMSG_GOODBYE] );
	FtpServer_OnClientDisconnect(pClient->m_pServer,pClient);
}

void FtpClient_OnCmdUser( FtpClient* pClient, const char* pUser )
{
	assert( pClient );

	if(!strcmp(pUser,"anonymous") && pClient->m_pServer->m_iAnonymous )
	{
		// anonymous login, user authenticated
		pClient->m_eAuthState = AUTHSTATE_VALID;

		FtpClient_Send(pClient,230, pClient->m_pMessages[FTPMSG_USER_LOGGED_IN] );
	}
	else
	{
		// allow password check
		pClient->m_eAuthState = !strcmp(pUser,pClient->m_pServer->m_Username) ? AUTHSTATE_PASSWORD : AUTHSTATE_FAKEPASS;

		FtpClient_Send(pClient,331, pClient->m_pMessages[FTPMSG_PASSWORD_REQUIRED_FOR_USER] ); // after this line, username is destroyed
	}
}

void FtpClient_OnCmdPass( FtpClient* pClient, const char* pPass )
{
	if( (AUTHSTATE_PASSWORD == pClient->m_eAuthState) && !strcmp(pPass,pClient->m_pServer->m_Password) )
	{
		// password matches, allow login
		FtpClient_Send(pClient,230,"User logged in.");
		pClient->m_eAuthState = AUTHSTATE_VALID;
	}
	else
	{
		// password didn't match, or we had no valid login

		if( AUTHSTATE_INVALID != pClient->m_eAuthState )
		{
			FtpClient_Send(pClient,530, pClient->m_pMessages[FTPMSG_LOGIN_INCORRECT] );

			// disconnect client if more than 3 attempts to login has been made

			pClient->m_iAuthAttempt++;
			if( pClient->m_iAuthAttempt > 3 )
				FtpServer_OnClientDisconnect( pClient->m_pServer, pClient );

			pClient->m_eAuthState = AUTHSTATE_INVALID;
		}
		else
			FtpClient_Send( pClient, 503, pClient->m_pMessages[FTPMSG_LOGIN_WITH_USER_FIRST] );
	}
}

void FtpClient_OnCmdPasv( FtpClient* pClient )
{
	struct sockaddr_in sa;
	socklen_t sl;
	int s;
	unsigned int addr;
	unsigned short port;
	char* buf;

	assert( pClient );

	FtpClient_OnDataCleanup(pClient);

	// get socket address we will listen to

	sl = sizeof(sa);
	if( getsockname( pClient->m_iControlSocket, (struct sockaddr*)&sa,&sl ) < 0 )
	{
		FtpClient_Send( pClient, 500, pClient->m_pMessages[FTPMSG_COULD_NOT_ENTER_PASSIVE_MODE] );
		return;
	}
	sa.sin_port = 0;

	if( (s = socket( AF_INET, SOCK_STREAM, 0)) < 0 )
  {
  	disconnect(s);
		FtpClient_Send( pClient, 500, pClient->m_pMessages[FTPMSG_COULD_NOT_ENTER_PASSIVE_MODE] );
		return;
	}

	if( bind( s, (struct sockaddr*)&sa, sl ) < 0 )
	{
  	disconnect(s);
		FtpClient_Send( pClient, 500, pClient->m_pMessages[FTPMSG_COULD_NOT_ENTER_PASSIVE_MODE] );
		return;
	}

	if( listen( s, 1 ) < 0 )
  {
  	disconnect(s);
		FtpClient_Send( pClient, 500, pClient->m_pMessages[FTPMSG_COULD_NOT_ENTER_PASSIVE_MODE] );
		return;
	}
                
	// report back

	sl = sizeof(sa);
	if( getsockname( s, (struct sockaddr*)&sa,&sl ) < 0 )
	{
		FtpClient_Send( pClient, 500, pClient->m_pMessages[FTPMSG_COULD_NOT_ENTER_PASSIVE_MODE] );
		return;
	}

	// store socket and flag us as listening

	pClient->m_iDataSocket = s;
	pClient->m_eConnState = CONNSTATE_LISTEN;

	// generate reply

	addr = htonl(sa.sin_addr.s_addr);
	port = htons(sa.sin_port);

	buf = buffer;
	strcpy( buf, pClient->m_pMessages[FTPMSG_ENTERING_PASSIVE_MODE] );
	strcat(buf," (");
	itoa(buf+strlen(buf),(addr>>24)&0xff);
	strcat(buf,",");
	itoa(buf+strlen(buf),(addr>>16)&0xff);
	strcat(buf,",");
	itoa(buf+strlen(buf),(addr>>8)&0xff);
	strcat(buf,",");
	itoa(buf+strlen(buf),(addr)&0xff);
	strcat(buf,",");
	itoa(buf+strlen(buf),port>>8);
	strcat(buf,",");
	itoa(buf+strlen(buf),port&0xff);
	strcat(buf,").");
/*
	sprintf( buffer, "Entering passive mode (%d,%d,%d,%d,%d,%d).",
										(addr>>24)&0xff, (addr>>16)&0xff, (addr>>8)&0xff, addr&0xff,
										port>>8,port&0xff );
*/

	FtpClient_Send( pClient, 227, buffer );
}

void FtpClient_OnCmdPort( FtpClient* pClient, int* ip, int port )
{
	assert( pClient );

	if( port >= 1024 )
	{
		// TODO: validate IP?

		pClient->m_iRemoteAddress[0] = ip[0];
		pClient->m_iRemoteAddress[1] = ip[1];
		pClient->m_iRemoteAddress[2] = ip[2];
		pClient->m_iRemoteAddress[3] = ip[3];

		pClient->m_iRemotePort = port;

		FtpClient_Send( pClient, 200, pClient->m_pMessages[FTPMSG_COMMAND_SUCCESSFUL] );
	}
	else
		FtpClient_Send( pClient, 500, pClient->m_pMessages[FTPMSG_ILLEGAL_COMMAND] );
}

void FtpClient_OnCmdSyst( FtpClient* pClient )
{
	assert( pClient );

	// TODO: is this proper?

	/* MS-style LIST format */
	FtpClient_Send( pClient, 215, "Windows_NT version 5.0" );

	/* UNIX-style LIST format: To use uncomment this format after commenting out MS-style LIST format 
		and making changes in FtpClient.c's FtpClient_OnDataWrite
	FtpClient_Send( pClient, 215, "UNIX Type: L8" );
	*/
}

void FtpClient_OnCmdList( struct FtpClient* pClient, const char* pPath, int iNamesOnly )
{
	assert( pClient );

	if( iNamesOnly )
		pClient->m_eDataAction = DATAACTION_NLST;
	else
		pClient->m_eDataAction = DATAACTION_LIST;

	// attempt to open directory

	if( FileSystem_OpenDir(&pClient->m_kContext,pPath) < 0 )
	{
		FtpClient_Send( pClient, 500, pClient->m_pMessages[FTPMSG_UNABLE_TO_OPEN_DIRECTORY] );
		return;
	}

	FtpClient_HandleDataConnect( pClient );
}

void FtpClient_OnCmdType( FtpClient* pClient, const char* pType )
{
	if( (tolower(*pType) == 'i')||(tolower(*pType) == 'a') )
		FtpClient_Send( pClient, 200, pClient->m_pMessages[FTPMSG_COMMAND_SUCCESSFUL] );
	else
		FtpClient_Send( pClient, 500, pClient->m_pMessages[FTPMSG_ILLEGAL_COMMAND] );
}

void FtpClient_OnCmdMode( FtpClient* pClient, const char* pMode )
{
	// only stream-mode supported

	if( tolower(*pMode) == 's' )
		FtpClient_Send( pClient, 200, pClient->m_pMessages[FTPMSG_COMMAND_SUCCESSFUL] );
	else
		FtpClient_Send( pClient, 500, pClient->m_pMessages[FTPMSG_COMMAND_FAILED] );
}

void FtpClient_OnCmdStru( FtpClient* pClient, const char* pStructure )
{
	// only file-structure supported

	if( tolower(*pStructure) == 'f' )
		FtpClient_Send( pClient, 200, pClient->m_pMessages[FTPMSG_COMMAND_SUCCESSFUL] );
	else
		FtpClient_Send( pClient, 500, pClient->m_pMessages[FTPMSG_COMMAND_FAILED] );
}

void FtpClient_OnCmdRetr( FtpClient* pClient, const char* pFile )
{
	int iMarker = pClient->m_iRestartMarker;
	pClient->m_iRestartMarker = 0;

	if( FileSystem_OpenFile( &pClient->m_kContext, pFile, FM_READ, iMarker ) < 0 )
	{
		FtpClient_Send( pClient, 500, pClient->m_pMessages[FTPMSG_FILE_NOT_FOUND] );
		return;
	}

	pClient->m_eDataAction = DATAACTION_RETR;

	FtpClient_HandleDataConnect( pClient );
}

void FtpClient_OnCmdStor( FtpClient* pClient, const char* pFile )
{
	int iMarker = pClient->m_iRestartMarker;
	pClient->m_iRestartMarker = 0;

	if( FileSystem_OpenFile( &pClient->m_kContext, pFile, FM_WRITE, iMarker ) < 0 )
	{
		FtpClient_Send( pClient, 500, pClient->m_pMessages[FTPMSG_COULD_NOT_CREATE_FILE] );
		return;
	}

	pClient->m_eDataAction = DATAACTION_STOR;

	FtpClient_HandleDataConnect( pClient );
}

void FtpClient_OnCmdAppe( FtpClient* pClient, const char* pFile )
{
	assert( pClient );

	if( FileSystem_OpenFile( &pClient->m_kContext, pFile, FM_WRITE, -1 ) < 0 )
	{
		FtpClient_Send( pClient, 500, pClient->m_pMessages[FTPMSG_COULD_NOT_CREATE_FILE] );
		return;
	}

	pClient->m_eDataAction = DATAACTION_APPE;

	FtpClient_HandleDataConnect( pClient );
}

void FtpClient_OnCmdRest( FtpClient* pClient, int iMarker )
{
	if( iMarker < 0 )
	{
		pClient->m_iRestartMarker = 0;
		FtpClient_Send( pClient, 501, pClient->m_pMessages[FTPMSG_INVALID_RESTART_MARKER] );
	}
	else
	{
		pClient->m_iRestartMarker = iMarker;
		FtpClient_Send( pClient, 350, pClient->m_pMessages[FTPMSG_RESTART_MARKER_SET] );
	}
}

void FtpClient_OnCmdPwd( FtpClient* pClient )
{
	char* buf2 = buffer;

	*(buf2++) = '\"';
	*(buf2) = '\0';
	strcat( buf2, pClient->m_kContext.m_Path );
	buf2 = buf2+strlen(buf2);
	*(buf2++) = '\"';
	*(buf2) = '\0';

	FtpClient_Send( pClient, 257, buffer );
}

void FtpClient_OnCmdCwd( FtpClient* pClient, const char* pPath )
{
	if( FileSystem_ChangeDir(&pClient->m_kContext,pPath) < 0 )
		FtpClient_Send( pClient, 550, pClient->m_pMessages[FTPMSG_COMMAND_FAILED] );
	else
		FtpClient_Send( pClient, 250, pClient->m_pMessages[FTPMSG_COMMAND_SUCCESSFUL] );
}

void FtpClient_OnCmdDele( FtpClient* pClient, const char* pFile )
{
	if( FileSystem_DeleteFile(&pClient->m_kContext,pFile) < 0 )
		FtpClient_Send( pClient, 550, pClient->m_pMessages[FTPMSG_COMMAND_FAILED] );
	else
		FtpClient_Send( pClient, 250, pClient->m_pMessages[FTPMSG_COMMAND_SUCCESSFUL] );
}

void FtpClient_OnCmdMkd( FtpClient* pClient, const char* pDir )
{
	if( FileSystem_CreateDir(&pClient->m_kContext,pDir) < 0 )
		FtpClient_Send( pClient, 550, pClient->m_pMessages[FTPMSG_COMMAND_FAILED] );
	else
		FtpClient_Send( pClient, 250, pClient->m_pMessages[FTPMSG_COMMAND_SUCCESSFUL] );
}

void FtpClient_OnCmdRmd( FtpClient* pClient, const char* pDir )
{
	if( FileSystem_DeleteDir(&pClient->m_kContext,pDir) < 0 )
		FtpClient_Send( pClient, 550, pClient->m_pMessages[FTPMSG_COMMAND_FAILED] );
	else
		FtpClient_Send( pClient, 250, pClient->m_pMessages[FTPMSG_COMMAND_SUCCESSFUL] );
}

void FtpClient_OnCmdRnfr( FtpClient* pClient, const char* name )
{
	int iMarker = pClient->m_iRestartMarker;
	pClient->m_iRestartMarker = 0;

	// check if file/dir exists that is to be renamed
	if( (FileSystem_OpenFile( &pClient->m_kContext, name, FM_READ, iMarker)) && (FileSystem_OpenDir(&pClient->m_kContext, name)) < 0 )
		FtpClient_Send( pClient, 550, pClient->m_pMessages[FTPMSG_COMMAND_FAILED] );
	else
	{
		// get just the name not the path
		FileSystem_BuildPath( buffer, pClient->m_kContext.m_Path, name );
		if( NULL != (name = FileSystem_ClassifyPath( &pClient->m_kContext, buffer )) )
			strcpy(buffer, name);
		FtpClient_Send( pClient, 350, pClient->m_pMessages[FTPMSG_COMMAND_SUCCESSFUL] );
	}
}

void FtpClient_OnCmdRnto( FtpClient* pClient, const char* newName )
{
	if( FileSystem_Rename(&pClient->m_kContext,buffer,newName) < 0 )
		FtpClient_Send( pClient, 550, pClient->m_pMessages[FTPMSG_COMMAND_FAILED] );
	else
		FtpClient_Send( pClient, 250, pClient->m_pMessages[FTPMSG_COMMAND_SUCCESSFUL] );
}

void FtpClient_OnCmdSize( FtpClient* pClient, const char* pFile )
{
	int size = FileSystem_GetFileSize( &(pClient->m_kContext), pFile );

	if( size < 0 )
		FtpClient_Send( pClient, 550, pClient->m_pMessages[FTPMSG_COULD_NOT_GET_FILESIZE] );
	else
	{
		itoa( buffer, size );
		FtpClient_Send( pClient, 213, buffer );
	}
}

void FtpClient_OnCmdSite( FtpClient* pClient, const char* pCmd )
{
	char* c;

	// copy command to clean buffer
	strcpy(buffer,pCmd);

	c = strtok(buffer," ");

	if(c)
	{
		int i = 0;
		unsigned int result = 0;

		for( i = 0; *c && i < 4; i++ )
			result = (result << 8)|tolower(*(c++));
	
		switch( result )
		{
#ifndef LINUX
			// SITE MNT <device> <file>
			case SITECMD_MNT:
			{
				char* mount_point;
				char* mount_file;

				// get mount point
				mount_point = strtok(NULL," ");
				if(!mount_point||!strlen(mount_point))
				{
					FtpClient_Send( pClient, 500, pClient->m_pMessages[FTPMSG_REQUIRES_PARAMETERS] );
					break;
				}

				// get mount source
				mount_file = strtok(NULL,"");
				if(!mount_file||!strlen(mount_file))
				{
					FtpClient_Send( pClient, 500, pClient->m_pMessages[FTPMSG_REQUIRES_PARAMETERS] );
					break;
				}

				FtpClient_OnSiteMount(pClient, mount_point,mount_file);
			}
			break;

			// SITE UMNT <device>
			case SITECMD_UMNT:
			{
				char* mount_point = strtok(NULL,"");

				if(mount_point)
					FtpClient_OnSiteUmount(pClient,mount_point);
				else
					FtpClient_Send( pClient, 500, pClient->m_pMessages[FTPMSG_REQUIRES_PARAMETERS] );
			}
			break;

			// SITE SYNC <device>
			case SITECMD_SYNC:
			{
				char* devname = strtok(NULL,"");

				if(devname)
					FtpClient_OnSiteSync( pClient, devname );
				else
					FtpClient_Send( pClient, 500, pClient->m_pMessages[FTPMSG_REQUIRES_PARAMETERS] );
			}
			break;
#endif

			default:
			{
				FtpClient_Send( pClient, 500, pClient->m_pMessages[FTPMSG_NOT_SUPPORTED] );
			}
			break;
		}
	}
	else
		FtpClient_Send( pClient, 500, pClient->m_pMessages[FTPMSG_NOT_UNDERSTOOD] );

}

#ifndef LINUX
void FtpClient_OnSiteMount( struct FtpClient* pClient, const char* pMountPoint, const char* pMountFile )
{
	// mount filesystem

	if( FileSystem_MountDevice( &(pClient->m_kContext), pMountPoint, pMountFile ) < 0 )
		FtpClient_Send( pClient, 550, pClient->m_pMessages[FTPMSG_COMMAND_FAILED] );
	else
		FtpClient_Send( pClient, 214, pClient->m_pMessages[FTPMSG_COMMAND_SUCCESSFUL] );
}

void FtpClient_OnSiteUmount( struct FtpClient* pClient, const char* pMountPoint )
{
	// unmount filesystem

	if( FileSystem_UnmountDevice( &(pClient->m_kContext), pMountPoint ) < 0 )
		FtpClient_Send( pClient, 550, pClient->m_pMessages[FTPMSG_COMMAND_FAILED] );
	else
		FtpClient_Send( pClient, 214, pClient->m_pMessages[FTPMSG_COMMAND_SUCCESSFUL] );

}

void FtpClient_OnSiteSync( struct FtpClient* pClient, const char* pDeviceName )
{
	// sync data with filesystem

	if( FileSystem_SyncDevice( &(pClient->m_kContext), pDeviceName ) < 0 )
		FtpClient_Send( pClient, 550, pClient->m_pMessages[FTPMSG_COMMAND_FAILED] );
	else
		FtpClient_Send( pClient, 214, pClient->m_pMessages[FTPMSG_COMMAND_SUCCESSFUL] );
}
#endif
