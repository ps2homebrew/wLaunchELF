#.SILENT:

SMB ?= 0
SIO_DEBUG ?= 0
#set SMB to 1 to build uLe with smb support

EE_BIN = BOOT-UNC.ELF
EE_BIN_PKD = BOOT.ELF
EE_OBJS = main.o pad.o config.o elf.o draw.o loader_elf.o filer.o \
	poweroff_irx.o iomanx_irx.o filexio_irx.o ps2atad_irx.o ps2dev9_irx.o ps2ip_irx.o netman_irx.o \
	ps2smap_irx.o ps2hdd_irx.o ps2fs_irx.o ps2netfs_irx.o usbd_irx.o bdm_irx.o bdmfs_fatfs_irx.o usbmass_bd_irx.o mcman_irx.o mcserv_irx.o\
	extflash_irx.o xfromman_irx.o \
	dvrdrv_irx.o dvrfile_irx.o \
	cdfs_irx.o ps2ftpd_irx.o ps2host_irx.o vmc_fs_irx.o ps2kbd_irx.o\
	hdd.o hdl_rpc.o hdl_info_irx.o editor.o timer.o jpgviewer.o icon.o lang.o\
	font_uLE.o makeicon.o chkesr.o allowdvdv_irx.o

ifeq ($(SMB),1)
	EE_OBJS += smbman.o
endif

EE_OBJS_DIR = obj/
EE_ASM_DIR = asm/
EE_OBJS := $(EE_OBJS:%=$(EE_OBJS_DIR)%)

EE_INCS := -I$(PS2DEV)/gsKit/include -I$(PS2SDK)/ports/include

EE_LDFLAGS := -L$(PS2DEV)/gsKit/lib -L$(PS2SDK)/ports/lib -s
EE_LIBS = -lgskit -ldmakit -ljpeg_ps2_addons -ljpeg -lpad -lmc -lhdd -lkbd -lm \
	-lcdvd -lfileXio -lpatches -lpoweroff -ldebug -lsior
EE_CFLAGS := -mno-gpopt -G0

ifeq ($(SIO_DEBUG),1)
	EE_CFLAGS += -DSIO_DEBUG
	EE_OBJS += sior_irx.o
endif

ifeq ($(SMB),1)
	EE_CFLAGS += -DSMB
endif

BIN2C = $(PS2SDK)/bin/bin2c

.PHONY: all run reset clean rebuild format format-check

all: githash.h $(EE_BIN_PKD)

$(EE_BIN_PKD): $(EE_BIN)
	ps2-packer $< $@

run: all
	ps2client -h 192.168.0.10 -t 1 execee host:$(EE_BIN)
reset: clean
	ps2client -h 192.168.0.10 reset

format:
	find . -type f -a \( -iname \*.h -o -iname \*.c \) | xargs clang-format -i

format-check:
	@! find . -type f -a \( -iname \*.h -o -iname \*.c \) | xargs clang-format -style=file -output-replacements-xml | grep "<replacement " >/dev/null

githash.h:
	printf '#ifndef ULE_VERDATE\n#define ULE_VERDATE "' > $@ && \
	git show -s --format=%cd --date=local | tr -d "\n" >> $@ && \
	printf '"\n#endif\n' >> $@
	printf '#ifndef GIT_HASH\n#define GIT_HASH "' >> $@ && \
	git rev-parse --short HEAD | tr -d "\n" >> $@ && \
	printf '"\n#endif\n' >> $@

$(EE_ASM_DIR)mcman_irx.c: $(PS2SDK)/iop/irx/mcman-old.irx | $(EE_ASM_DIR)
	$(BIN2C) $< $@ mcman_irx

$(EE_ASM_DIR)mcserv_irx.c: $(PS2SDK)/iop/irx/mcserv-old.irx | $(EE_ASM_DIR)
	$(BIN2C) $< $@ mcserv_irx

$(EE_ASM_DIR)dvrdrv_irx.c: $(PS2SDK)/iop/irx/dvrdrv.irx | $(EE_ASM_DIR)
	$(BIN2C) $< $@ dvrdrv_irx

$(EE_ASM_DIR)dvrfile_irx.c: $(PS2SDK)/iop/irx/dvrfile.irx | $(EE_ASM_DIR)
	$(BIN2C) $< $@ dvrfile_irx

$(EE_ASM_DIR)extflash_irx.c: $(PS2SDK)/iop/irx/extflash.irx | $(EE_ASM_DIR)
	$(BIN2C) $< $@ extflash_irx

$(EE_ASM_DIR)xfromman_irx.c: $(PS2SDK)/iop/irx/xfromman.irx | $(EE_ASM_DIR)
	$(BIN2C) $< $@ xfromman_irx

$(EE_ASM_DIR)usbd_irx.c: $(PS2SDK)/iop/irx/usbd.irx | $(EE_ASM_DIR)
	$(BIN2C) $< $@ usbd_irx

$(EE_ASM_DIR)bdm_irx.c: $(PS2SDK)/iop/irx/bdm.irx | $(EE_ASM_DIR)
	$(BIN2C) $< $@ bdm_irx

$(EE_ASM_DIR)bdmfs_fatfs_irx.c: $(PS2SDK)/iop/irx/bdmfs_fatfs.irx | $(EE_ASM_DIR)
	$(BIN2C) $< $@ bdmfs_fatfs_irx

$(EE_ASM_DIR)usbmass_bd_irx.c: $(PS2SDK)/iop/irx/usbmass_bd.irx | $(EE_ASM_DIR)
	$(BIN2C) $< $@ usbmass_bd_irx

$(EE_ASM_DIR)cdfs_irx.c: $(PS2SDK)/iop/irx/cdfs.irx | $(EE_ASM_DIR)
	$(BIN2C) $< $@ cdfs_irx

$(EE_ASM_DIR)poweroff_irx.c: $(PS2SDK)/iop/irx/poweroff.irx | $(EE_ASM_DIR)
	$(BIN2C) $< $@ poweroff_irx

$(EE_ASM_DIR)iomanx_irx.c: $(PS2SDK)/iop/irx/iomanX.irx | $(EE_ASM_DIR)
	$(BIN2C) $< $@ iomanx_irx

$(EE_ASM_DIR)filexio_irx.c: $(PS2SDK)/iop/irx/fileXio.irx | $(EE_ASM_DIR)
	$(BIN2C) $< $@ filexio_irx

$(EE_ASM_DIR)ps2dev9_irx.c: $(PS2SDK)/iop/irx/ps2dev9.irx | $(EE_ASM_DIR)
	$(BIN2C) $< $@ ps2dev9_irx

$(EE_ASM_DIR)ps2ip_irx.c: $(PS2SDK)/iop/irx/ps2ip-nm.irx | $(EE_ASM_DIR)
	$(BIN2C) $< $@ ps2ip_irx

$(EE_ASM_DIR)netman_irx.c: $(PS2SDK)/iop/irx/netman.irx | $(EE_ASM_DIR)
	$(BIN2C) $< $@ netman_irx

$(EE_ASM_DIR)ps2smap_irx.c: $(PS2SDK)/iop/irx/smap.irx | $(EE_ASM_DIR)
	$(BIN2C) $< $@ ps2smap_irx

oldlibs/ps2ftpd/ps2ftpd.irx: oldlibs/ps2ftpd
	$(MAKE) -C $<

$(EE_ASM_DIR)ps2ftpd_irx.c: oldlibs/ps2ftpd/ps2ftpd.irx | $(EE_ASM_DIR)
	$(BIN2C) $< $@ ps2ftpd_irx

$(EE_ASM_DIR)ps2atad_irx.c: $(PS2SDK)/iop/irx/ps2atad.irx | $(EE_ASM_DIR)
	$(BIN2C) $< $@ ps2atad_irx

$(EE_ASM_DIR)ps2hdd_irx.c: $(PS2SDK)/iop/irx/ps2hdd-osd.irx | $(EE_ASM_DIR)
	$(BIN2C) $< $@ ps2hdd_irx

$(EE_ASM_DIR)ps2fs_irx.c: $(PS2SDK)/iop/irx/ps2fs.irx | $(EE_ASM_DIR)
	$(BIN2C) $< $@ ps2fs_irx

$(EE_ASM_DIR)ps2netfs_irx.c: $(PS2SDK)/iop/irx/ps2netfs.irx | $(EE_ASM_DIR)
	$(BIN2C) $< $@ ps2netfs_irx

hdl_info/hdl_info.irx: hdl_info
	$(MAKE) -C $<

$(EE_ASM_DIR)hdl_info_irx.c: hdl_info/hdl_info.irx | $(EE_ASM_DIR)
	$(BIN2C) $< $@ hdl_info_irx

ps2host/ps2host.irx: ps2host
	$(MAKE) -C $<

$(EE_ASM_DIR)ps2host_irx.c: ps2host/ps2host.irx | $(EE_ASM_DIR)
	$(BIN2C) $< $@ ps2host_irx

ifeq ($(SMB),1)
$(EE_ASM_DIR)smbman_irx.c: $(PS2SDK)/iop/irx/smbman.irx | $(EE_ASM_DIR)
	$(BIN2C) $< $@ smbman_irx
endif

vmc_fs/vmc_fs.irx: vmc_fs
	$(MAKE) -C $<

$(EE_ASM_DIR)vmc_fs_irx.c: vmc_fs/vmc_fs.irx | $(EE_ASM_DIR)
	$(BIN2C) $< $@ vmc_fs_irx

loader/loader.elf: loader
	$(MAKE) -C $<

$(EE_ASM_DIR)loader_elf.c: loader/loader.elf | $(EE_ASM_DIR)
	$(BIN2C) $< $@ loader_elf

$(EE_ASM_DIR)ps2kbd_irx.c: $(PS2SDK)/iop/irx/ps2kbd.irx | $(EE_ASM_DIR)
	$(BIN2C) $< $@ ps2kbd_irx

$(EE_ASM_DIR)sior_irx.c: $(PS2SDK)/iop/irx/sior.irx | $(EE_ASM_DIR)
	$(BIN2C) $< $@ sior_irx

AllowDVDV/AllowDVDV.irx: AllowDVDV
	$(MAKE) -C $<

$(EE_ASM_DIR)allowdvdv_irx.c: AllowDVDV/AllowDVDV.irx | $(EE_ASM_DIR)
	$(BIN2C) $< $@ allowdvdv_irx

clean:
	$(MAKE) -C hdl_info clean
	$(MAKE) -C ps2host clean
	$(MAKE) -C loader clean
	$(MAKE) -C vmc_fs clean
	$(MAKE) -C AllowDVDV clean
	$(MAKE) -C oldlibs/ps2ftpd clean
	rm -rf githash.h $(EE_BIN) $(EE_BIN_PKD) $(EE_ASM_DIR) $(EE_OBJS_DIR)

rebuild: clean all

$(EE_ASM_DIR):
	@mkdir -p $@

$(EE_OBJS_DIR):
	@mkdir -p $@

$(EE_OBJS_DIR)%.o: $(EE_SRC_DIR)%.c | $(EE_OBJS_DIR)
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

$(EE_OBJS_DIR)%.o: $(EE_ASM_DIR)%.c | $(EE_OBJS_DIR)
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
