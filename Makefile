EE_BIN = BOOT.ELF
EE_OBJS = main.o pad.o config.o elf.o draw.o loader.o  filer.o mass_rpc.o cd.o\
	poweroff.o iomanx.o filexio.o ps2atad.o ps2dev9.o ps2ip.o ps2smap.o ps2hdd.o\
	ps2fs.o ps2netfs.o usbd.o usb_mass.o cdvd.o ps2ftpd.o ps2host.o fakehost.o  \
	ps2kbd.o hdd.o hdl_rpc.o hdl_info.o editor.o timer.o jpgviewer.o icon.o

EE_INCS := -I$(PS2DEV)/libito/include -I$(PS2DEV)/libjpg/include\
	-I$(PS2SDK)/sbv/include -I$(PS2DEV)/libcdvd/ee

EE_LDFLAGS := -L$(PS2DEV)/libito/lib -L$(PS2DEV)/libjpg\
	-L$(PS2SDK)/sbv/lib -L$(PS2DEV)/libcdvd/lib -s
EE_LIBS = -lpad -lito -ljpg -lmc -lhdd -lcdvdfs -lkbd -lmf -lfileXio -lpatches -lpoweroff  -ldebug -lc

all:	$(EE_BIN)
	rm -f hdl_info.s

run: all
	ps2client -h 192.168.0.10 -t 1 execee host:BOOT.ELF
reset: clean
	ps2client -h 192.168.0.10 reset

usbd.s:
	bin2s $(PS2SDK)/iop/irx/usbd.irx usbd.s usbd_irx

usb_mass.s:
	bin2s modules/usb_mass.irx usb_mass.s usb_mass_irx

cdvd.s:
#	bin2s $(PS2DEV)/libcdvd/lib/cdvd.irx cdvd.s cdvd_irx
	bin2s modules/cdvd.irx cdvd.s cdvd_irx

poweroff.s:
	bin2s $(PS2SDK)/iop/irx/poweroff.irx poweroff.s poweroff_irx

iomanx.s:
	bin2s $(PS2SDK)/iop/irx/iomanX.irx iomanx.s iomanx_irx

filexio.s:
	bin2s $(PS2SDK)/iop/irx/fileXio.irx filexio.s filexio_irx

ps2dev9.s:
	bin2s $(PS2SDK)/iop/irx/ps2dev9.irx ps2dev9.s ps2dev9_irx

ps2ip.s:
	bin2s $(PS2SDK)/iop/irx/ps2ip.irx ps2ip.s ps2ip_irx

ps2smap.s:
	bin2s $(PS2ETH)/bin/ps2smap.irx ps2smap.s ps2smap_irx

ps2ftpd.s:
	bin2s modules/ps2ftpd.irx ps2ftpd.s ps2ftpd_irx

ps2atad.s:
	bin2s $(PS2SDK)/iop/irx/ps2atad.irx ps2atad.s ps2atad_irx

ps2hdd.s:
	bin2s $(PS2SDK)/iop/irx/ps2hdd.irx ps2hdd.s ps2hdd_irx

ps2fs.s:
	bin2s $(PS2SDK)/iop/irx/ps2fs.irx ps2fs.s ps2fs_irx

ps2netfs.s:
	bin2s $(PS2SDK)/iop/irx/ps2netfs.irx ps2netfs.s ps2netfs_irx

fakehost.s:
	bin2s $(PS2SDK)/iop/irx/fakehost.irx fakehost.s fakehost_irx

hdl_info.s:
	$(MAKE) -C hdl_info
	bin2s hdl_info/hdl_info.irx hdl_info.s hdl_info_irx

ps2host.s:
	$(MAKE) -C ps2host
	bin2s ps2host/ps2host.irx ps2host.s ps2host_irx

loader.s:
	$(MAKE) -C loader
	bin2s loader/loader.elf loader.s loader_elf

ps2kbd.s:
	bin2s $(PS2SDK)/iop/irx/ps2kbd.irx ps2kbd.s ps2kbd_irx

clean:
	$(MAKE) -C hdl_info clean
	$(MAKE) -C ps2host clean
	$(MAKE) -C loader clean
	rm -f *.o *.a *.s BOOT.ELF

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
