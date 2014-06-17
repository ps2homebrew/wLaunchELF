EE_BIN = BOOT.ELF
EE_OBJS = main.o pad.o config.o elf.o draw.o loader.o  filer.o mass_rpc.o cd.o\
	poweroff.o iomanx.o filexio.o ps2atad.o ps2dev9.o ps2hdd.o ps2fs.o ps2netfs.o\
	usbd.o usb_mass.o cdvd.o

EE_INCS := -I$(PS2DEV)/libito/include -I$(PS2DEV)/sbv-1.0-lite/include\
	-I$(PS2DEV)/libcdvd/ee

EE_LDFLAGS := -L$(PS2DEV)/libito/lib -L$(PS2DEV)/sbv-1.0-lite/lib\
	-L$(PS2DEV)/libcdvd/lib -s
EE_LIBS = -lpad -lito -lmc -lhdd -lcdvdfs -lfileXio -lsbv_patches -lpoweroff  -ldebug -lc

all: $(EE_BIN)

usbd.s:
	bin2s modules/usbd.irx usbd.s usbd_irx

usb_mass.s:
	bin2s modules/usb_mass.irx usb_mass.s usb_mass_irx

cdvd.s:
	bin2s $(PS2DEV)/libcdvd/lib/cdvd.irx cdvd.s cdvd_irx

poweroff.s:
	bin2s $(PS2SDK)/iop/irx/poweroff.irx poweroff.s poweroff_irx

iomanx.s:
	bin2s $(PS2SDK)/iop/irx/iomanX.irx iomanx.s iomanx_irx

filexio.s:
	bin2s $(PS2SDK)/iop/irx/fileXio.irx filexio.s filexio_irx

ps2dev9.s:
	bin2s modules/ps2dev9.irx ps2dev9.s ps2dev9_irx

ps2atad.s:
	bin2s $(PS2SDK)/iop/irx/ps2atad.irx ps2atad.s ps2atad_irx

ps2hdd.s:
	bin2s $(PS2SDK)/iop/irx/ps2hdd.irx ps2hdd.s ps2hdd_irx

ps2fs.s:
	bin2s $(PS2SDK)/iop/irx/ps2fs.irx ps2fs.s ps2fs_irx

ps2netfs.s:
	bin2s $(PS2SDK)/iop/irx/ps2netfs.irx ps2netfs.s ps2netfs_irx

loader.s:
	bin2s loader/loader.elf loader.s loader_elf

clean:
	del /f *.o *.a *.s

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
