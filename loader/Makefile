 EE_INCS := $(EE_INCS) -I$(PS2SDK)/sbv/include

EE_CFLAGS  := -mips3 -ffreestanding -fno-builtin -G0 \
		-fshort-double -mlong64 -mhard-float -mno-abicalls -O2 -EL -Wall \
		-c $(EE_INCS) $(EE_CFLAGS)

EE_ASFLAGS := -EL -G0 $(EE_ASFLAGS)

###dlanor: adjust these three, for PS2Link compatibility tests
###dlanor: old LaunchELF used LA=0x90000, SA=0xB0000, SS=0x08000
###dlanor: The values below were implemented in LaunchELF v3.50
LOADADDR  = 0x90000
STACKADDR = 0xA8000
STACKSIZE = 0x04000

ifeq ($(DEBUG),1)
LOADADDR  = 0x1700000
STACKADDR = 0x1720000
STACKSIZE = 0x08000
endif

LDPARAMS := -Wl,--defsym -Wl,_stack_size=$(STACKSIZE) -Wl,--defsym -Wl,_stack=$(STACKADDR)

EE_LDFLAGS += -Wl,-Ttext -Wl,$(LOADADDR) -s $(LDPARAMS)

EE_LDFLAGS := $(EE_LDFLAGS) -L$(PS2SDK)/sbv/lib \

EE_BIN = loader.elf

EE_OBJS = loader.o

EE_LIBS = -lfileXio -lpatches
ifeq ($(DEBUG),1)
EE_LIBS += -ldebug
endif

all: $(EE_BIN)

clean:
	rm -f *.o *.a *.s *.elf

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
