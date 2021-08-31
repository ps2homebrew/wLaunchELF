IOP_BIN = AllowDVDV.irx
IOP_OBJS = AllowDVDV.o imports.o

IOP_CFLAGS += -Wall -fno-builtin -D_IOP
IOP_LDFLAGS += -s
IOP_INCS += -I$(PS2SDK)/iop/include -I$(PS2SDK)/common/include

all: $(IOP_BIN)

clean:
	rm -f $(IOP_OBJS_DIR) $(IOP_BIN)

include $(PS2SDK)/Defs.make
include ../Rules.make
