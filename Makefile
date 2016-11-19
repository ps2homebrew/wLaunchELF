.SILENT:

SMB = 0
#set SMB to 1 to build uLe with smb support

EE_BIN = BOOT.ELF
EE_BIN_PKD = ULE.ELF
EE_OBJS = main.o pad.o config.o elf.o draw.o loader.bin.o filer.o \
	poweroff.bin.o iomanx.bin.o filexio.bin.o ps2atad.bin.o ps2dev9.bin.o smsutils.bin.o ps2ip.bin.o\
	ps2smap.bin.o ps2hdd.bin.o ps2fs.bin.o ps2netfs.bin.o usbd.bin.o usbhdfsd.bin.o mcman.bin.o mcserv.bin.o\
	cdvd.bin.o ps2ftpd.bin.o ps2host.bin.o vmc_fs.bin.o fakehost.bin.o ps2kbd.bin.o\
	hdd.o hdl_rpc.o hdl_info.bin.o editor.o timer.o jpgviewer.o icon.o lang.o\
	font_uLE.o makeicon.o chkesr.o sior.bin.o allowdvdv.bin.o
ifeq ($(SMB),1)
	EE_OBJS += smbman.bin.o
endif


EE_INCS := -I$(PS2DEV)/gsKit/include -I$(PS2SDK)/ports/include -Ioldlibs/libcdvd/ee

EE_LDFLAGS := -L$(PS2DEV)/gsKit/lib -L$(PS2SDK)/ports/lib -Loldlibs/bin -s
EE_LIBS = -lpad -lgskit -ldmakit -ljpeg -lmc -lhdd -lcdvdfs -lkbd -lmf -lcdvd -lc -lfileXio -lpatches -lpoweroff -ldebug -lc -lsior
ifeq ($(SMB),1)
	EE_CFLAGS += -DSMB
endif

all:	$(EE_BIN)
		ps2-packer $(EE_BIN) $(EE_BIN_PKD)

run: all
	ps2client -h 192.168.0.10 -t 1 execee host:BOOT.ELF
reset: clean
	ps2client -h 192.168.0.10 reset

mcman.bin.c:
	bin2c $(PS2SDK)/iop/irx/mcman.irx $@ mcman_irx

mcserv.bin.c:
	bin2c $(PS2SDK)/iop/irx/mcserv.irx $@ mcserv_irx

usbd.bin.c:
	bin2c $(PS2SDK)/iop/irx/usbd.irx $@ usbd_irx

usbhdfsd.bin.c:
	bin2c $(PS2SDK)/iop/irx/usbhdfsd.irx $@ usb_mass_irx

cdvd.bin.c:
	bin2c oldlibs/bin/cdvd.irx $@ cdvd_irx

poweroff.bin.c:
	bin2c $(PS2SDK)/iop/irx/poweroff.irx $@ poweroff_irx

iomanx.bin.c:
	bin2c $(PS2SDK)/iop/irx/iomanX.irx $@ iomanx_irx

filexio.bin.c:
	bin2c $(PS2SDK)/iop/irx/fileXio.irx $@ filexio_irx

ps2dev9.bin.c:
	bin2c $(PS2SDK)/iop/irx/ps2dev9.irx $@ ps2dev9_irx

ps2ip.bin.c:
	bin2c oldlibs/bin/SMSTCPIP.irx $@ ps2ip_irx

ps2smap.bin.c:
	bin2c oldlibs/bin/SMSMAP.irx $@ ps2smap_irx

smsutils.bin.c:
	bin2c oldlibs/bin/SMSUTILS.irx $@ smsutils_irx

ps2ftpd.bin.c:
	bin2c oldlibs/bin/ps2ftpd.irx $@ ps2ftpd_irx

ps2atad.bin.c:
	bin2c $(PS2SDK)/iop/irx/ps2atad.irx $@ ps2atad_irx

ps2hdd.bin.c:
	bin2c $(PS2SDK)/iop/irx/ps2hdd.irx $@ ps2hdd_irx

ps2fs.bin.c:
	bin2c $(PS2SDK)/iop/irx/ps2fs.irx $@ ps2fs_irx

ps2netfs.bin.c:
	bin2c $(PS2SDK)/iop/irx/ps2netfs.irx $@ ps2netfs_irx

fakehost.bin.c:
	bin2c $(PS2SDK)/iop/irx/fakehost.irx $@ fakehost_irx

hdl_info.bin.c:
	$(MAKE) -C hdl_info
	bin2c hdl_info/hdl_info.irx $@ hdl_info_irx

ps2host.bin.c:
	$(MAKE) -C ps2host
	bin2c ps2host/ps2host.irx $@ ps2host_irx

ifeq ($(SMB),1)
smbman.bin.c:
	bin2c $(PS2SDK)/iop/irx/smbman.irx $@ smbman_irx
endif

vmc_fs.bin.c:
	$(MAKE) -C vmc_fs
	bin2c vmc_fs/vmc_fs.irx $@ vmc_fs_irx

loader.bin.c:
	$(MAKE) -C loader
	bin2c loader/loader.elf $@ loader_elf

ps2kbd.bin.c:
	bin2c $(PS2SDK)/iop/irx/ps2kbd.irx $@ ps2kbd_irx

sior.bin.c:
	bin2c $(PS2SDK)/iop/irx/sior.irx $@ sior_irx

allowdvdv.bin.c:
	$(MAKE) -C AllowDVDV
	bin2c AllowDVDV/AllowDVDV.irx $@ allowdvdv_irx

clean:
	$(MAKE) -C hdl_info clean
	$(MAKE) -C ps2host clean
	$(MAKE) -C loader clean
	$(MAKE) -C vmc_fs clean
	$(MAKE) -C AllowDVDV clean
	rm -f *.bin.c $(EE_OBJS) $(EE_BIN) $(EE_BIN_PKD)

rebuild: clean all

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
