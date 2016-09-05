IOP_BIN  = vmc_fs.irx
IOP_OBJS = vmc_fs.o vmc_io.o vmc_misc.o vmc_fat.o vmc_ps2.o imports.o

IOP_CFLAGS  += -Wall -fno-builtin
IOP_LDFLAGS += -s

all: $(IOP_BIN)

clean:
	rm -f $(IOP_OBJS) $(IOP_BIN)

include $(PS2SDK)/Defs.make
include ../Rules.make
