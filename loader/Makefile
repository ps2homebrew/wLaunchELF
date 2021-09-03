
EE_CFLAGS = -D_EE -Os -G0 -Wall
EE_CFLAGS += -fdata-sections -ffunction-sections
EE_LDFLAGS = -Tlinkfile -Wl,-zmax-page-size=128
EE_LDFLAGS += -s -Wl,--gc-sections

EE_BIN = loader.elf

EE_OBJS = loader.o

EE_LIBS =
ifeq ($(DEBUG),1)
EE_LIBS += -ldebug
endif

all: $(EE_BIN)

clean:
	rm -f -r $(EE_OBJS_DIR) $(EE_BIN)

include $(PS2SDK)/Defs.make
include Rules.make
