#ifndef _MASS_RPC_H
#define _MASS_RPC_H

typedef struct _fat_dir_record { // 140 bytes
	unsigned char attr;		//attributes (bits:5-Archive 4-Directory 3-Volume Label 2-System 1-Hidden 0-Read Only)
	unsigned char name[128];
	unsigned char date[4];	//D:M:Yl:Yh
	unsigned char time[3];  //H:M:S
	unsigned int  size;	//file size, 0 for directory
} fat_dir_record;


int usb_mass_bindRpc();
int usb_mass_getFirstDirentry(char* path, fat_dir_record* record);
int usb_mass_getNextDirentry(fat_dir_record* record);
int usb_mass_dumpSystemInfo();
int usb_mass_dumpDiskContent(unsigned int startSector, unsigned int sectorCount, char* fname);
#endif /* _MASS_RPC_H */
