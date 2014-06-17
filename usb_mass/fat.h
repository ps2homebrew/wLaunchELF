/*
 * fat.h - USB Mass storage driver for PS2
 *
 * (C) 2004, Marek Olejnik (ole00@post.cz)
 *
 * FAT helper structures
 *
 * See the file LICENSE included with this distribution for licensing terms.
 */

/*
   FAT References: http://www.igd.fhg.de/~aschaefe/fips/distrib/techinfo.txt
                 : http://home.freeuk.net/foxy2k/disk/disk1.htm
*/

#ifndef _FAT_H
#define _FAT_H 1


#define FAT12 0x0C
#define FAT16 0x10
#define FAT32 0x20


#define FAT_MAX_PATH 1024
#define FAT_MAX_NAME 128

#define DIR_CHAIN_SIZE 32

typedef struct _part_raw_record {
	unsigned char	active;		//Set to 80h if this partition is active / bootable
	unsigned char	startH;		//Partition's starting head.
	unsigned char   startST[2];	//Partition's starting sector and track.
	unsigned char   sid;		//Partition's system ID number.
	unsigned char   endH;		//Partition's ending head.
	unsigned char   endST[2];	//Partition's ending sector and track.
	unsigned char   startLBA[4];	//Starting LBA (sector)
	unsigned char	size[4];	//Partition size in sectors.
} part_raw_record;

typedef struct _part_record {
	unsigned char sid;		//system id - 4=16bit FAT (16bit sector numbers)
					//            5=extended partition
					//            6=16bit FAT (32bit sector numbers)
        unsigned int  start;		// start sector of the partition
        unsigned int  count;		// length of the partititon (total number of sectors)
} part_record;


typedef struct _fat_part {
	part_record  record[4];		//maximum of 4 primary partitions
} fat_part;



/* bios parameter block - bpb - fat12, fat16  */
typedef struct _fat_raw_bpb {
	unsigned char jump[3];		//jump instruction ('eb xx 90' or 'e9 xx xx')
	unsigned char oem[8];		//OEM name and version - e.g. MSDOS5.0
	unsigned char sectorSize[2];	//bytes per sector - should be 512
	unsigned char clusterSize;	//sectors per cluster - power of two
	unsigned char resSectors[2];	//reserved sectors - typically 1 (boot sector)
	unsigned char fatCount;		//number of FATs - must be 2
	unsigned char rootSize[2];	//number of rootdirectory entries - typically 512
	unsigned char sectorCountO[2];	//number of sectors (short) - 0, if BIGDOS partition
	unsigned char mediaDesc;	//media descriptor - typically f8h
	unsigned char fatSize[2];	//sectors per FAT - varies
	unsigned char trackSize[2];	//sectors per track
	unsigned char headCount[2];	//number of heads
	unsigned char hiddenCountL[2];	//number of hidden sectors (low)

	unsigned char hiddenCountH[2];	//number of hidden sectors (high)
	unsigned char sectorCount[4];	//number of sectors

	/* extended BPB since DOS 4.0 */
	unsigned char driveNumber;	//physical drive number - 80h or 81h
	unsigned char reserved;		//Current head (not used for this but WinNT stores two flags here).
	unsigned char signature;	//Signature (must be 28h or 29h to be recognised by NT).
	unsigned char serialNumber[4]; //The serial number, the serial number is stored in reverse 
					//order and is the hex representation of the bytes stored here
	unsigned char volumeLabel[11];	//Volume label
	unsigned char fatId[8];		//File system ID. "FAT12", "FAT16" or "FAT  "
} fat_raw_bpb;

/* bios parameter block - bpb - fat32 */
typedef struct _fat32_raw_bpb {
	unsigned char jump[3];		//jump instruction ('eb xx 90' or 'e9 xx xx')
	unsigned char oem[8];		//OEM name and version - e.g. MSDOS5.0
	unsigned char sectorSize[2];	//bytes per sector - should be 512
	unsigned char clusterSize;	//sectors per cluster - power of two
	unsigned char resSectors[2];	//reserved sectors - typically 1 (boot sector)
	unsigned char fatCount;		//number of FATs - must be 2
	unsigned char rootSize[2];	//number of rootdirectory entries - typically 512
	unsigned char sectorCountO[2];	//number of sectors (short) - 0, if BIGDOS partition
	unsigned char mediaDesc;	//media descriptor - typically f8h
	unsigned char fatSize[2];	//sectors per FAT - varies
	unsigned char trackSize[2];	//sectors per track
	unsigned char headCount[2];	//number of heads
	unsigned char hiddenCountL[2];	//number of hidden sectors (low)

	unsigned char hiddenCountH[2];	//number of hidden sectors (high)
	unsigned char sectorCount[4];	//number of sectors

	/* fat32 specific */
	unsigned char fatSize32[4];	//FAT32 sectors per FAT
	unsigned char fatStatus[2];	//If bit 7 is clear then all FAT's are updated other wise bits 0-3
					//give the current active FAT, all other bits are reserved.
	unsigned char revision[2];	//High byte is major revision number, low byte is minor revision number, currently both are 0
	unsigned char rootDirCluster[4];//Root directory starting cluster
	unsigned char fsInfoSector[2];	//File system information sector.
	unsigned char bootSectorCopy[2];//If non-zero this gives the sector which holds a copy of the boot record, usually 6
	unsigned char reserved1[12];	//Reserved, set to 0.
	unsigned char pdn;		//Physical drive number (BIOS system ie 80h is first HDD, 00h is first FDD)
	unsigned char reserved2;	//Reserved
	unsigned char signature;	//Signature (must be 28h or 29h to be recognised by NT)
	unsigned char serialNumber[4]; //The serial number, the serial number is stored in reverse 
					//order and is the hex representation of the bytes stored here
	unsigned char volumeLabel[11];	//Volume label
	unsigned char fatId[8];		//File system ID. "FAT12", "FAT16" or "FAT  "
	unsigned char machineCode[8];	//Machine code
	unsigned char bootSignature[2];	//Boot Signature AA55h.

} fat32_raw_bpb;

typedef struct _fat_bpb {
	unsigned int  sectorSize;	//bytes per sector - should be 512
	unsigned char clusterSize;	//sectors per cluster - power of two
	unsigned int  resSectors;	//reserved sectors - typically 1 (boot sector)
	unsigned char fatCount;		//number of FATs - must be 2
	unsigned int  rootSize;		//number of rootdirectory entries - typically 512
	unsigned int  fatSize;		//sectors per FAT - varies
	unsigned int  trackSize;	//sectors per track
	unsigned int  headCount;	//number of heads
	unsigned int  hiddenCount;	//number of hidden sectors 
	unsigned int  sectorCount;	//number of sectors
	unsigned int  partStart;	//sector where partition starts (boot sector)
	unsigned int  rootDirStart;	//sector where root directory starts
	unsigned int  rootDirCluster;	//fat32 - cluster of the root directory
	unsigned int  activeFat;	//fat32 - current active fat number
	unsigned char fatType;		//12-FAT16, 16-FAT16, 32-FAT32
	unsigned char fatId[9];		//File system ID. "FAT12", "FAT16" or "FAT  " - for debug only
} fat_bpb;


/* directory entry of the short file name */
typedef struct _fat_direntry_sfn {
	unsigned char name[8];		//Filename padded with spaces if required.
	unsigned char ext[3];		//Filename extension padded with spaces if required.
	unsigned char attr;		//File Attribute Byte.

	unsigned char reservedNT;	//Reserved for use by Windows NT.
	unsigned char seconds;		//Tenths of a second at time of file creation, 0-199 is valid.
	unsigned char timeCreate[2];	//Time when file was created.
	unsigned char dateCreate[2]; 	//Date when file was created.
	unsigned char dateAccess[2];	//Date when file was last accessed.
	unsigned char clusterH[2];	//High word of cluster number (always 0 for FAT12 and FAT16).

	unsigned char timeWrite[2];	//Time of last write to file (last modified or when created).
	unsigned char dateWrite[2];	//Date of last write to file (last modified or when created).
	unsigned char clusterL[2];	//Starting cluster (Low word).
	unsigned char size[4];		//File size (set to zero if a directory).
} fat_direntry_sfn;


/* directory entry of the long file name

   Each LFN directory entry holds 13 characters of the complete LFN using 16-bit Unicode characters.
*/
typedef struct _fat_direntry_lfn {
	unsigned char entrySeq;		//Bits 0-5 give the LFN part number, bit 6 is set if this is the last entry for the file.	
	unsigned char name1[10];	//1st 5 letters of LFN entry.
	unsigned char rshv;		//?? 0Fh (RSHV attributes set)

	unsigned char reserved1;	//Reserved set to 0.
	unsigned char checksum; 	//Checksum generated from SFN.
	unsigned char name2[12];	//Next 6 letters of LFN entry.

	unsigned char reserved2[2];	//Reserved set to 0.
	unsigned char name3[4];		//Last 2 letters of LFN entry.
} fat_direntry_lfn;


typedef struct _fat_direntry {
	unsigned char attr;		//Attributes (bits:5-Archive 4-Directory 3-Volume Label 2-System 1-Hidden 0-Read Only)
	unsigned char name[FAT_MAX_NAME];//Long name (zero terminated)
	unsigned char sname[13];	//Short name (zero terminated)
	unsigned int  size;		//file size, 0 for directory
	unsigned int  cluster;		//file start cluster 
} fat_direntry;


typedef struct _fat_dir_chain_record {
	unsigned int cluster;
	unsigned int index;
} fat_dir_chain_record;

typedef struct _fat_dir {
	unsigned char attr;		//attributes (bits:5-Archive 4-Directory 3-Volume Label 2-System 1-Hidden 0-Read Only)
	unsigned char name[FAT_MAX_NAME];
	unsigned char cdate[4];	//D:M:Yl:Yh
	unsigned char ctime[3]; //H:M:S
	unsigned char adate[4];	//D:M:Yl:Yh
	unsigned char atime[3]; //H:M:S
	unsigned char mdate[4];	//D:M:Yl:Yh
	unsigned char mtime[3]; //H:M:S
	unsigned int  size;		//file size, 0 for directory
	unsigned int  lastCluster;
	fat_dir_chain_record  chain[DIR_CHAIN_SIZE];  //cluser/offset cache - for seeking purpose
} fat_dir;

typedef struct _fat_dir_record { // 140 bytes
	unsigned char attr;		//attributes (bits:5-Archive 4-Directory 3-Volume Label 2-System 1-Hidden 0-Read Only)
	unsigned char name[FAT_MAX_NAME];
	unsigned char date[4];	//D:M:Yl:Yh
	unsigned char time[3];  //H:M:S
	unsigned int  size;		//file size, 0 for directory
} fat_dir_record;
#endif /* _FAT_H */
