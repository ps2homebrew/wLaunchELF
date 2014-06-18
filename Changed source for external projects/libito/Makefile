EE_SRC_DIR = source/
EE_LIB_DIR = lib/

EE_CFLAGS := -D_EE -O2 -G0 -Wall $(EE_CFLAGS)

EE_OBJS = itogs.o itogsprims.o itodma.o itosyscalls.o itopacket.o itoimage.o ito.o itomisc.o

EE_INCS = -I$(LIBITO)/include
EE_LIB = lib/libito.a

EE_OBJS := $(EE_OBJS:%=$(EE_SRC_DIR)%)

all: $(EE_SRC_DIR) $(EE_LIB_DIR) $(EE_LIB)

build-examples:
	@echo Building examples
	$(MAKE) -C examples

clean:
	rm -f $(EE_SRC_DIR)*.o $(EE_LIB_DIR)*.a

clean-examples:
	$(MAKE) -C examples clean

$(EE_SRC_DIR)%.o : $(EE_SRC_DIR)%.c
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

$(EE_LIB) : $(EE_OBJS)
	$(EE_AR) cru $(EE_LIB) $(EE_OBJS)

include $(PS2SDK)/Defs.make
