# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004.
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.

IOP_OBJS_DIR = obj/
IOP_BIN_DIR = bin/
IOP_SRC_DIR = src/
IOP_INC_DIR = include/

IOP_BIN = ps2ftpd.irx
IOP_OBJS = main.o FileSystem.o FtpServer.o FtpClient.o FtpCommands.o FtpMessages.o imports.o

IOP_CFLAGS += -Iinclude -Isrc
# -DDEBUG

all: $(IOP_BIN)

clean:
	rm -f -r $(IOP_OBJS_DIR) $(IOP_BIN_DIR)

include $(PS2SDK)/Defs.make
include $(PS2SDK)/samples/Makefile.iopglobal
