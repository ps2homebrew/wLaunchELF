IOP_BIN_DIR  = bin/
IOP_INC_DIR  = include/
IOP_OBJS_DIR = obj/
IOP_SRC_DIR  = src/

IOP_BIN  = bin/SMSTCPIP.irx
IOP_OBJS = $(IOP_OBJS_DIR)ps2ip.o    \
           $(IOP_OBJS_DIR)inet.o     \
           $(IOP_OBJS_DIR)ip.o       \
           $(IOP_OBJS_DIR)ip_addr.o  \
           $(IOP_OBJS_DIR)ip_frag.o  \
           $(IOP_OBJS_DIR)etharp.o   \
           $(IOP_OBJS_DIR)tcp_in.o   \
           $(IOP_OBJS_DIR)tcp_out.o  \
           $(IOP_OBJS_DIR)tcp.o      \
           $(IOP_OBJS_DIR)tcpip.o    \
           $(IOP_OBJS_DIR)mem.o      \
           $(IOP_OBJS_DIR)api_lib.o  \
           $(IOP_OBJS_DIR)api_msg.o  \
           $(IOP_OBJS_DIR)exports.o  \
           $(IOP_OBJS_DIR)imports.o  \
           $(IOP_OBJS_DIR)sockets.o  \
           $(IOP_OBJS_DIR)netif.o    \
           $(IOP_OBJS_DIR)udp.o      \
           $(IOP_OBJS_DIR)memp.o     \
           $(IOP_OBJS_DIR)icmp.o     \
           $(IOP_OBJS_DIR)pbuf.o

all: $(IOP_OBJS_DIR) $(IOP_BIN_DIR) $(IOP_BIN)

clean:
	rm -rf $(IOP_OBJS_DIR) $(IOP_BIN_DIR)

include $(PS2SDKSRC)/Defs.make
include $(PS2SDKSRC)/iop/Rules.make
include $(PS2SDKSRC)/iop/Rules.release

IOP_CFLAGS  += -DLWIP_NOASSERT -Wall -fno-builtin-printf
IOP_LDFLAGS += -s
