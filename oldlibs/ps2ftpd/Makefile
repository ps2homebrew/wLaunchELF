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

IOP_BIN = bin/ps2ftpd.irx
IOP_OBJS = obj/main.o obj/FileSystem.o obj/FtpServer.o obj/FtpClient.o obj/FtpCommands.o obj/FtpMessages.o obj/imports.o

IOP_CFLAGS += -Wall -fno-builtin
# -DDEBUG
IOP_LDFLAGS += -s
IOP_INCS += -I$(PS2SDK)/iop/include

all: $(IOP_OBJS_DIR) $(IOP_BIN_DIR) $(IOP_BIN)

clean:
	rm -f -r $(IOP_OBJS_DIR) $(IOP_BIN_DIR)

include $(PS2SDK)/Defs.make
include Rules.make
