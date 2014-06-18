IOP_BIN = AllowDVDV.irx
IOP_OBJS = AllowDVDV.o imports.o

IOP_INCS += -I$(PS2SDK)/iop/include
IOP_CFLAGS += -Wall -fno-builtin
IOP_LDFLAGS += -s

all: $(IOP_BIN)
	-rm -f *.o

include $(PS2SDK)/Defs.make
include Rules.make
