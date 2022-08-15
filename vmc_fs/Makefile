IOP_BIN  = vmc_fs.irx
IOP_OBJS = vmc_fs.o vmc_io.o vmc_misc.o vmc_fat.o vmc_ps2.o imports.o

all: $(IOP_BIN)

clean:
	rm -f -r $(IOP_OBJS_DIR) $(IOP_BIN)

include $(PS2SDK)/Defs.make
include $(PS2SDK)/samples/Makefile.iopglobal
