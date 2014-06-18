IOP_OBJS_DIR = obj/
IOP_BIN_DIR = bin/
IOP_SRC_DIR = src/

IOP_BIN  = bin/vmcfs.irx
IOP_OBJS = obj/vmc_fs.o obj/vmc_io.o obj/vmc_misc.o obj/vmc_fat.o obj/vmc_ps2.o obj/imports.o

IOP_CFLAGS  += -Wall -fno-builtin
IOP_LDFLAGS += -s

all: $(IOP_OBJS_DIR) $(IOP_BIN_DIR) $(IOP_BIN)

clean:
	rm -f -r $(IOP_OBJS_DIR) $(IOP_BIN_DIR)

include $(PS2SDKSRC)/Defs.make
include $(PS2SDKSRC)/iop/Rules.make
include $(PS2SDKSRC)/iop/Rules.release