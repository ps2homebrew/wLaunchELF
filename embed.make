#keep embedded stuff here to avoid polluting Makefile 
mcman_irx.s: $(PS2SDK)/iop/irx/mcman-old.irx
	$(BIN2S) $< $@ mcman_irx

mcserv_irx.s: $(PS2SDK)/iop/irx/mcserv-old.irx
	$(BIN2S) $< $@ mcserv_irx

dvrdrv_irx.s: $(PS2SDK)/iop/irx/dvrdrv.irx
	$(BIN2S) $< $@ dvrdrv_irx

dvrfile_irx.s: $(PS2SDK)/iop/irx/dvrfile.irx
	$(BIN2S) $< $@ dvrfile_irx

usbd_irx.s: $(PS2SDK)/iop/irx/usbd.irx
	$(BIN2S) $< $@ usbd_irx

bdm_irx.s: $(PS2SDK)/iop/irx/bdm.irx
	$(BIN2S) $< $@ bdm_irx

bdmfs_fatfs_irx.s: $(PS2SDK)/iop/irx/bdmfs_fatfs.irx
	$(BIN2S) $< $@ bdmfs_fatfs_irx

usbmass_bd_irx.s: $(PS2SDK)/iop/irx/usbmass_bd.irx
	$(BIN2S) $< $@ usbmass_bd_irx

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

oldlibs/ps2ftpd/ps2ftpd.irx: oldlibs/ps2ftpd
	$(MAKE) -C $<

ps2ftpd_irx.s: oldlibs/ps2ftpd/ps2ftpd.irx
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
