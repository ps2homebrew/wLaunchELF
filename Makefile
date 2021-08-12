#.SILENT:

SMB = 0
#set SMB to 1 to build uLe with smb support

EE_BIN = BOOT-UNC.ELF
EE_BIN_PKD = BOOT.ELF
EE_OBJS = main.o pad.o config.o elf.o draw.o loader_elf.o filer.o \
	poweroff_irx.o iomanx_irx.o filexio_irx.o ps2atad_irx.o ps2dev9_irx.o ps2ip_irx.o netman_irx.o \
	ps2smap_irx.o ps2hdd_irx.o ps2fs_irx.o ps2netfs_irx.o usbd_irx.o usbhdfsd_irx.o mcman_irx.o mcserv_irx.o\
	cdfs_irx.o ps2ftpd_irx.o ps2host_irx.o vmc_fs_irx.o ps2kbd_irx.o\
	hdd.o hdl_rpc.o hdl_info_irx.o editor.o timer.o jpgviewer.o icon.o lang.o\
	font_uLE.o makeicon.o chkesr.o sior_irx.o allowdvdv_irx.o
ifeq ($(SMB),1)
	EE_OBJS += smbman.o
endif

EE_INCS := -I$(PS2DEV)/gsKit/include -I$(PS2SDK)/ports/include

EE_LDFLAGS := -L$(PS2DEV)/gsKit/lib -L$(PS2SDK)/ports/lib -s
EE_LIBS = -lgskit -ldmakit -ljpeg_ps2_addons -ljpeg -lpad -lmc -lhdd -lkbd -lm \
	-lcdvd -lfileXio -lpatches -lpoweroff -ldebug -lsior
EE_CFLAGS := -mno-gpopt -G0

ifeq ($(SMB),1)
	EE_CFLAGS += -DSMB
endif

BIN2S = $(PS2SDK)/bin/bin2s

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

mcman_irx.s: $(PS2SDK)/iop/irx/mcman.irx
	$(BIN2S) $< $@ mcman_irx

mcserv_irx.s: $(PS2SDK)/iop/irx/mcserv.irx
	$(BIN2S) $< $@ mcserv_irx

usbd_irx.s: $(PS2SDK)/iop/irx/usbd.irx
	$(BIN2S) $< $@ usbd_irx

usbhdfsd_irx.s: $(PS2SDK)/iop/irx/usbhdfsd.irx
	$(BIN2S) $< $@ usb_mass_irx

cdfs_irx.s: $(PS2SDK)/iop/irx/cdfs.irx
	$(BIN2S) $< $@ cdfs_irx

poweroff_irx.s: $(PS2SDK)/iop/irx/poweroff.irx
	$(BIN2S) $< $@ poweroff_irx

iomanx_irx.s: $(PS2SDK)/iop/irx/iomanX.irx
	$(BIN2S) $< $@ iomanx_irx

filexio_irx.s: $(PS2SDK)/iop/irx/fileXio.irx
	$(BIN2S) $< $@ filexio_irx

ps2dev9_irx.s: $(PS2SDK)/iop/irx/ps2dev9.irx
	$(BIN2S) $< $@ ps2dev9_irx

ps2ip_irx.s: $(PS2SDK)/iop/irx/ps2ip-nm.irx
	$(BIN2S) $< $@ ps2ip_irx

netman_irx.s: $(PS2SDK)/iop/irx/netman.irx
	$(BIN2S) $< $@ netman_irx

ps2smap_irx.s: $(PS2SDK)/iop/irx/smap.irx
	$(BIN2S) $< $@ ps2smap_irx

oldlibs/ps2ftpd/bin/ps2ftpd.irx: oldlibs/ps2ftpd
	$(MAKE) -C $<

ps2ftpd_irx.s: oldlibs/ps2ftpd/bin/ps2ftpd.irx
	$(BIN2S) $< $@ ps2ftpd_irx

ps2atad_irx.s: $(PS2SDK)/iop/irx/ps2atad.irx
	$(BIN2S) $< $@ ps2atad_irx

ps2hdd_irx.s: $(PS2SDK)/iop/irx/ps2hdd-osd.irx
	$(BIN2S) $< $@ ps2hdd_irx

ps2fs_irx.s: $(PS2SDK)/iop/irx/ps2fs.irx
	$(BIN2S) $< $@ ps2fs_irx

ps2netfs_irx.s: $(PS2SDK)/iop/irx/ps2netfs.irx
	$(BIN2S) $< $@ ps2netfs_irx

hdl_info/hdl_info.irx: hdl_info
	$(MAKE) -C $<

hdl_info_irx.s: hdl_info/hdl_info.irx
	$(BIN2S) $< $@ hdl_info_irx

ps2host/ps2host.irx: ps2host
	$(MAKE) -C $<

ps2host_irx.s: ps2host/ps2host.irx
	$(BIN2S) $< $@ ps2host_irx

ifeq ($(SMB),1)
smbman_irx.s: $(PS2SDK)/iop/irx/smbman.irx
	$(BIN2S) $< $@ smbman_irx
endif

vmc_fs/vmc_fs.irx: vmc_fs
	$(MAKE) -C $<

vmc_fs_irx.s: vmc_fs/vmc_fs.irx
	$(BIN2S) $< $@ vmc_fs_irx

loader/loader.elf: loader
	$(MAKE) -C $<

loader_elf.s: loader/loader.elf
	$(BIN2S) $< $@ loader_elf

ps2kbd_irx.s: $(PS2SDK)/iop/irx/ps2kbd.irx
	$(BIN2S) $< $@ ps2kbd_irx

sior_irx.s: $(PS2SDK)/iop/irx/sior.irx
	$(BIN2S) $< $@ sior_irx

AllowDVDV/AllowDVDV.irx: AllowDVDV
	$(MAKE) -C $<

allowdvdv_irx.s: AllowDVDV/AllowDVDV.irx
	$(BIN2S) $< $@ allowdvdv_irx

clean:
	$(MAKE) -C hdl_info clean
	$(MAKE) -C ps2host clean
	$(MAKE) -C loader clean
	$(MAKE) -C vmc_fs clean
	$(MAKE) -C AllowDVDV clean
	$(MAKE) -C oldlibs/ps2ftpd clean
	rm -f githash.h *.s $(EE_OBJS) $(EE_BIN) $(EE_BIN_PKD)

rebuild: clean all

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
