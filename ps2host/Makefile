
IOP_BIN  = ps2host.irx
IOP_OBJS = net_fsys.o net_fio.o ps2host.o imports.o

IOP_INCS += -I../include

all: $(IOP_BIN)

clean:
	-rm -f -r $(IOP_OBJS_DIR) $(IOP_BIN)

include $(PS2SDK)/Defs.make
include $(PS2SDK)/samples/Makefile.iopglobal
