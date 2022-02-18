/*
 * main.c - FTP startup
 *
 * Copyright (C) 2004 Jesper Svennevid
 *
 * Licensed under the AFL v2.0. See the file LICENSE included with this
 * distribution for licensing terms.
 *
 * c0cfb60b c7d2fe44 c3c9fc45
 */

#include "FtpServer.h"

#ifndef LINUX
#include "irx_imports.h"
#else
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#endif

int process_args(FtpServer *pServer, int argc, char *argv[])
{
    int i;

    for (i = 1; i < argc; i++) {
        int arg_avail = i < (argc - 1);

        if (!strcmp("-port", argv[i])) {
            if (!arg_avail)
                return -1;

            FtpServer_SetPort(pServer, strtol(argv[++i], NULL, 10));
        } else if (!strcmp("-anonymous", argv[i])) {
            // enable anonymous logins
            FtpServer_SetAnonymous(pServer, 1);
        } else if (!strcmp("-user", argv[i])) {
            // set new username

            if (!arg_avail)
                return -1;

            FtpServer_SetUsername(pServer, argv[++i]);
        } else if (!strcmp("-pass", argv[0])) {
            // set new password

            if (!arg_avail)
                return -1;

            FtpServer_SetPassword(pServer, argv[++i]);
        } else {
            printf("ps2ftpd: unknown argument '%s'\n", argv[i]);
            return -1;
        }
    }

    return 0;
}

#ifndef LINUX

// placed out here to end up on the heap, not the stack
FtpServer srv;

int server(void *argv)
{
    // run server

    while (FtpServer_IsRunning(&srv))
        FtpServer_HandleEvents(&srv);

    FtpServer_Destroy(&srv);
    return 0;
}

s32 _start(int argc, char *argv[])
{
    iop_thread_t mythread;
    int pid;
    int i;

    // TODO: fix CD/DVD support
    // printf("cdinit: %d\n",sceCdInit(CdMmodeDvd));

    printf("ps2ftpd: starting\n");

    // create server

    FtpServer_Create(&srv);

    // process arguments

    if (process_args(&srv, argc, argv) < 0) {
        FtpServer_Destroy(&srv);
        printf("ps2ftpd: could not parse arguments\n");
        return MODULE_NO_RESIDENT_END;
    }

    // setup server

    FtpServer_SetPort(&srv, 21);

    if (-1 == FtpServer_Start(&srv)) {
        FtpServer_Destroy(&srv);
        printf("ps2ftpd: could not create server\n");
        return MODULE_NO_RESIDENT_END;
    }

    // Start socket server thread

    mythread.attr = 0x02000000;        // attr
    mythread.option = 0;               // option
    mythread.thread = (void *)server;  // entry
    mythread.stacksize = 0x1000;
    mythread.priority = 0x43;  // just above ps2link

    pid = CreateThread(&mythread);

    if (pid > 0) {
        if ((i = StartThread(pid, NULL)) < 0) {
            FtpServer_Destroy(&srv);
            printf("ps2ftpd: StartThread failed (%d)\n", i);
            return MODULE_NO_RESIDENT_END;
        }
    } else {
        FtpServer_Destroy(&srv);
        printf("ps2ftpd: CreateThread failed (%d)\n", pid);
        return MODULE_NO_RESIDENT_END;
    }

    return MODULE_RESIDENT_END;
}


#else

int main(int argc, char *argv[])
{
    FtpServer srv;

    // create server

    FtpServer_Create(&srv);

    // process arguments

    FtpServer_SetPort(&srv, 2500);  // 21 privileged under linux
    if (process_args(&srv, argc, argv) < 0) {
        printf("ps2ftpd: could not parse arguments\n");
        FtpServer_Destroy(&srv);
        return 1;
    }

    // setup server

    if (-1 == FtpServer_Start(&srv)) {
        FtpServer_Destroy(&srv);
        return 1;
    }

    // run server

    while (FtpServer_IsRunning(&srv))
        FtpServer_HandleEvents(&srv);

    FtpServer_Destroy(&srv);
    return 0;
}

#endif
