IOP_BIN		= hdl_info.irx
IOP_OBJS	= main.o hdd.o apa.o hdl.o imports.o exports.o

all: $(IOP_BIN)

run: all
	ps2client -h 192.168.0.10 -t 1 execiop host:$(IOP_BIN)
reset: clean
	ps2client -h 192.168.0.10 reset

clean:
	rm -f -r $(IOP_OBJS) $(IOP_BIN)

include $(PS2SDK)/Defs.make
include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.iopglobal

