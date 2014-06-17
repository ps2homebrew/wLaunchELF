
IOP_BIN  = ps2host.irx
IOP_OBJS = net_fsys.o net_fio.o ps2host.o imports.o

IOP_INCS += -I../include
IOP_LIBS += 
IOP_LDFLAGS += 
IOP_CFLAGS += -Wall

all: $(IOP_BIN)

clean:
	-rm -f $(IOP_OBJS) $(IOP_BIN)

include $(PS2SDK)/Defs.make
include Rules.make
