IOP_LDFLAGS += -m mipsirx 

IOP_BIN = ../lib/cdvd.irx

IOP_OBJS = cdvd_iop.o iop_cdvdman.o


all: $(IOP_BIN)

clean:
	rm -f $(IOP_BIN) $(IOP_OBJS)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.iopglobal
