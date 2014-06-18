IOP_OBJS	= main.o hdd.o apa.o hdl.o imports.o exports.o

IOP_BIN		= hdl_info.irx

IOP_CFLAGS  += -Wall -fno-builtin
IOP_LDFLAGS += -s
IOP_INCS += -I$(PS2SDK)/iop/include -I$(PS2SDK)/common/include

all: $(IOP_BIN)

run: all
	ps2client -h 192.168.0.10 -t 1 execiop host:$(IOP_BIN)
reset: clean
	ps2client -h 192.168.0.10 reset

clean:
	rm -f *.o *.irx

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.iopglobal

# compiling with iop-gcc 3.2.2 causes weird problems in ps2ftpd
# temporary solution is to turn off compiler optimization

IOP_CFLAGS = $(CFLAGS_TARGET) -O2 -G0 -c $(IOP_INCS) -Wall -fno-builtin

# A rule to build imports.lst.
%.o : %.lst
	echo "#include \"irx_imports.h\"" > build-imports.c
	cat $< >> build-imports.c
	$(IOP_CC) $(IOP_CFLAGS) build-imports.c -o $@
	-rm -f build-imports.c

# A rule to build exports.tab.
%.o : %.tab
	echo "#include \"irx.h\"" > build-exports.c
	cat $< >> build-exports.c
	$(IOP_CC) $(IOP_CFLAGS) build-exports.c -o $@
	-rm -f build-exports.c
