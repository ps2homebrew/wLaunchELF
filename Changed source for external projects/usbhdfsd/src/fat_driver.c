#include <stdio.h>
#include <sysclib.h>
#include <sys/stat.h>

#include <thbase.h>
#include <errno.h>
#define malloc(a)       AllocSysMemory(0,(a), NULL)
#define free(a)         FreeSysMemory((a))

#include "scache.h"
#include "fat_driver.h"
#include "fat_write.h"
#include "usbhd_common.h"

//#define DEBUG

#include "mass_debug.h"

// Added by Hermes
extern unsigned Size_Sector; // store size of sector from usb mass
extern unsigned g_MaxLBA;

#define MEMCPY(a,b,c) memcpy((a),(b),(c))

#define SECTOR_SIZE Size_Sector //512 modified by Hermes

#define DISK_INIT(a,b)		scache_init((a), (b))
#define DISK_CLOSE		scache_close
#define READ_SECTOR(a, b)	scache_readSector((a), (void **)&b)
#define FLUSH_SECTORS		scache_flushSectors

int	mounted;	//disk mounted=1 not mounted=0
fat_part partTable;	//partition master record
fat_bpb  partBpb;	//partition bios parameter block

// modified by Hermes
unsigned char* sbuf; //sector buffer
unsigned int cbuf[MAX_DIR_CLUSTER]; //cluster index buffer // 2048 by Hermes

static int workPartition;

unsigned int  direntryCluster; //the directory cluster requested by getFirstDirentry
int direntryIndex; //index of the directory children

unsigned int  lastChainCluster;
int lastChainResult;

int getI32(unsigned char* buf)
{
	return (buf[0]  + (buf[1] <<8) + (buf[2] << 16) + (buf[3] << 24));
}

int getI32_2(unsigned char* buf1, unsigned char* buf2)
{
	return (buf1[0]  + (buf1[1] <<8) + (buf2[0] << 16) + (buf2[1] << 24));
}

int getI16(unsigned char* buf)
{
	return (buf[0] + (buf[1] <<8) );
}

int strEqual(unsigned char *s1, unsigned char* s2) {
    unsigned char u1, u2;
    for (;;) {
		u1 = *s1++;
		u2 = *s2++;
		if (u1 >64 && u1 < 91)  u1+=32;
		if (u2 >64 && u2 < 91)  u2+=32;

		if (u1 != u2) {
			return -1;
		}
		if (u1 == '\0') {
		    return 0;
		}
    }
}

inline unsigned int fat_cluster2sector1216(fat_bpb* bpb, unsigned int cluster)
{
	return  bpb->rootDirStart + (bpb->rootSize / (bpb->sectorSize>>5))+ (bpb->clusterSize * (cluster-2));
                           //modified by Hermes    ^ this work :)
}

inline unsigned int fat_cluster2sector32(fat_bpb* bpb, unsigned int cluster)
{
	return  bpb->rootDirStart + (bpb->clusterSize * (cluster-2));
}

inline unsigned int fat_cluster2sector(fat_bpb* bpb, unsigned int cluster)
{
	switch(bpb->fatType)
	{
		case FAT32: return fat_cluster2sector32(bpb, cluster);
		default:    return fat_cluster2sector1216(bpb, cluster);
	}
}

inline void fat_getPartitionRecord(part_raw_record* raw, part_record* rec)
{
	rec->sid = raw->sid;
	rec->start = getI32(raw->startLBA);
	rec->count = getI32(raw->size);
}

/*

   0x321, 0xABC

     byte| byte| byte|
   +--+--+--+--+--+--+
   |2 |1 |C |3 |A |B |
   +--+--+--+--+--+--+

*/

unsigned int fat_getClusterRecord12(unsigned char* buf, int type) {
	if (type) { //1
		return ((buf[1]<< 4) + (buf[0] >>4));
	} else { // 0
		return (((buf[1] & 0x0F) << 8) + buf[0]);
	}
}
// Get Cluster chain into <buf> buffer
// returns:
// 0    :if buf is full (bufSize entries) and more chain entries exist
// 1-n  :number of filled entries of the buf
// -1   :error

//for fat12
/* fat12 cluster records can overlap the edge of the sector so we need to detect and maintain
   these cases
*/
int fat_getClusterChain12(fat_bpb* bpb, unsigned int cluster, unsigned int* buf, int bufSize, int start) {
	int ret;
	int i;
	int recordOffset;
	int sectorSpan;
	int fatSector;
	int cont;
	int lastFatSector;
	unsigned char xbuf[4];

	cont = 1;
	lastFatSector = -1;
	i = 0;
	if (start) {
		buf[i] = cluster; //strore first cluster
		i++;
	}
	while(i < bufSize && cont) {
		recordOffset = (cluster * 3) / 2; //offset of the cluster record (in bytes) from the FAT start
		fatSector = recordOffset / bpb->sectorSize;
		sectorSpan = 0;
		if ((recordOffset % bpb->sectorSize) == (bpb->sectorSize - 1)) {
			sectorSpan = 1;
		}
		if (lastFatSector !=  fatSector || sectorSpan) {
				ret = READ_SECTOR(bpb->partStart + bpb->resSectors + fatSector, sbuf);
				if (ret < 0) {
					printf("FAT driver:Read fat12 sector failed! sector=%i! \n", bpb->partStart + bpb->resSectors + fatSector );
					return -1;
				}
				lastFatSector = fatSector;

				if (sectorSpan) {
					xbuf[0] = sbuf[bpb->sectorSize - 2];
					xbuf[1] = sbuf[bpb->sectorSize - 1];
					ret = READ_SECTOR(bpb->partStart + bpb->resSectors + fatSector + 1, sbuf);
					if (ret < 0) {
						printf("FAT driver:Read fat12 sector failed sector=%i! \n", bpb->partStart + bpb->resSectors + fatSector + 1);
						return -1;
					}
					xbuf[2] = sbuf[0];
					xbuf[3] = sbuf[1];
				}
		}
		if (sectorSpan) { // use xbuf as source buffer
			cluster = fat_getClusterRecord12(xbuf + (recordOffset % bpb->sectorSize) - (bpb->sectorSize-2), cluster % 2);
		} else { // use sector buffer as source buffer
			cluster = fat_getClusterRecord12(sbuf + (recordOffset % bpb->sectorSize), cluster % 2);
		}

		if (cluster >= 0xFF8) {
			cont = 0; //continue = false
		} else {
			buf[i] = cluster;
			i++;
		}
	}

	if (cont) { return 0; }

	return i;
}


//for fat16
int fat_getClusterChain16(fat_bpb* bpb, unsigned int cluster, unsigned int* buf, int bufSize, int start) {
	int ret;
	int i;
	int indexCount;
	int fatSector;
	int cont;
	int lastFatSector;

	cont = 1;
	indexCount = bpb->sectorSize / 2; //FAT16->2, FAT32->4
	lastFatSector = -1;
	i = 0;
	if (start) {
		buf[i] = cluster; //strore first cluster
		i++;
	}
	while(i < bufSize && cont) {
		fatSector = cluster / indexCount;
		if (lastFatSector !=  fatSector) {
				ret = READ_SECTOR(bpb->partStart + bpb->resSectors + fatSector,  sbuf);
				if (ret < 0) {
					printf("FAT driver:Read fat16 sector failed! sector=%i! \n", bpb->partStart + bpb->resSectors + fatSector );
					return -1;
				}

				lastFatSector = fatSector;
		}
		cluster = getI16(sbuf + ((cluster % indexCount) * 2));
		if (cluster >= 0xFFF8) {
			cont = 0; //continue = false
		} else {
			buf[i] = cluster;
			i++;
		}
	}
	if (cont) {
		return 0;
	} else {
		return i;
	}
}

//for fat32
int fat_getClusterChain32(fat_bpb* bpb, unsigned int cluster, unsigned int* buf, int bufSize, int start) {
	int ret;
	int i;
	int indexCount;
	int fatSector;
	int cont;
	int lastFatSector;

	cont = 1;
	indexCount = bpb->sectorSize / 4; //FAT16->2, FAT32->4
	lastFatSector = -1;
	i = 0;
	if (start) {
		buf[i] = cluster; //strore first cluster
		i++;
	}
	while(i < bufSize && cont) {
		fatSector = cluster / indexCount;
		if (lastFatSector !=  fatSector) {
				ret = READ_SECTOR(bpb->partStart + bpb->resSectors + fatSector,  sbuf);
				if (ret < 0) {
					printf("FAT driver: Read fat32 sector failed sector=%i! \n", bpb->partStart + bpb->resSectors + fatSector );
					return -1;
				}

				lastFatSector = fatSector;
		}
		cluster = getI32(sbuf + ((cluster % indexCount) * 4));
		if ((cluster & 0xFFFFFFF) >= 0xFFFFFF8) {
			cont = 0; //continue = false
		} else {
			buf[i] = cluster & 0xFFFFFFF;
			i++;
		}
	}
	if (cont) {
		return 0;
	} else {
		return i;
	}
}


int fat_getClusterChain(fat_bpb* bpb, unsigned int cluster, unsigned int* buf, int bufSize, int start) {

	if (cluster == lastChainCluster) {
		return lastChainResult;
	}

	switch (bpb->fatType) {
		case FAT12: lastChainResult = fat_getClusterChain12(bpb, cluster, buf, bufSize, start); break;
		case FAT16: lastChainResult = fat_getClusterChain16(bpb, cluster, buf, bufSize, start); break;
		case FAT32: lastChainResult = fat_getClusterChain32(bpb, cluster, buf, bufSize, start); break;
	}
	lastChainCluster = cluster;
	return lastChainResult;
}

void fat_invalidateLastChainResult()
{
	lastChainCluster  = 0;
}


void fat_getPartitionTable ( fat_part* part )
{
    part_raw_record* part_raw;
    int              i;
    int              ret;

    workPartition = -1;

    ret = READ_SECTOR( 0 , sbuf );  // read sector 0 - Disk MBR or boot sector
    if ( ret < 0 )
    {
        printf ( "FAT driver: Read sector 0 failed!\n" );
        return;
    }

    /* read 4 partition records */
    for ( i = 0; i < 4; i++)
    {
        part_raw = ( part_raw_record* )(  sbuf + 0x01BE + ( i * 16 )  );

        fat_getPartitionRecord ( part_raw, &part -> record[ i ] );

        if(
            part -> record[ i ].sid == 6    ||
            part -> record[ i ].sid == 4    ||
            part -> record[ i ].sid == 1    ||  // fat 16, fat 12
            part -> record[ i ].sid == 0x0B ||
            part -> record[ i ].sid == 0x0C ||  // fat 32
            part -> record[ i ].sid == 0x0E)    // fat 16 LBA
        {
            workPartition = i;
        }

        if ( workPartition == -1 )
        {  // no partition table detected
            // try to use "floppy" option
            workPartition = 0;
            part -> record[ 0 ].sid   =
            part -> record[ 0 ].start = 0;
            part -> record[ 0 ].count = g_MaxLBA;
        }
    }
}

void fat_determineFatType(fat_bpb* bpb) {
	int sector;
	int clusterCount;

	//get sector of cluster 0
	sector = fat_cluster2sector(bpb, 0);
	//remove partition start sector to get BR+FAT+ROOT_DIR sector count
	sector -= bpb->partStart;
	sector = bpb->sectorCount - sector;
	clusterCount = sector / bpb->clusterSize;
	//printf("Data cluster count = %i \n", clusterCount);

	if (clusterCount < 4085) {
		bpb->fatType = FAT12;
	} else
	if (clusterCount < 65525) {
		bpb->fatType = FAT16;
	} else {
		bpb->fatType = FAT32;
	}
}

void fat_getPartitionBootSector(part_record* part_rec, fat_bpb* bpb) {
	fat_raw_bpb* bpb_raw; //fat16, fat12
	fat32_raw_bpb* bpb32_raw; //fat32
	int ret;

	ret = READ_SECTOR(part_rec->start, sbuf); //read partition boot sector (first sector on partition)

	if (ret < 0) {
		printf("FAT driver: Read partition boot sector failed sector=%i! \n", part_rec->start);
		return;
	}

	bpb_raw = (fat_raw_bpb*) sbuf;
	bpb32_raw = (fat32_raw_bpb*) sbuf;

	//set fat common properties
	bpb->sectorSize	= getI16(bpb_raw->sectorSize);
	bpb->clusterSize = bpb_raw->clusterSize;
	bpb->resSectors = getI16(bpb_raw->resSectors);
	bpb->fatCount = bpb_raw->fatCount;
	bpb->rootSize = getI16(bpb_raw->rootSize);
	bpb->fatSize = getI16(bpb_raw->fatSize);
	bpb->trackSize = getI16(bpb_raw->trackSize);
	bpb->headCount = getI16(bpb_raw->headCount);
	bpb->hiddenCount = getI32(bpb_raw->hiddenCountL);
	bpb->sectorCount = getI16(bpb_raw->sectorCountO);
	if (bpb->sectorCount == 0) {
		bpb->sectorCount = getI32(bpb_raw->sectorCount); // large partition
	}
	bpb->partStart = part_rec->start;
	bpb->rootDirStart = part_rec->start + (bpb->fatCount * bpb->fatSize) + bpb->resSectors;
	for (ret = 0; ret < 8; ret++) {
		bpb->fatId[ret] = bpb_raw->fatId[ret];
	}
	bpb->fatId[ret] = 0;
	bpb->rootDirCluster = 0;

	fat_determineFatType(bpb);

	//fat32 specific info
	if (bpb->fatType == FAT32 && bpb->fatSize == 0) {
		bpb->fatSize = getI32(bpb32_raw->fatSize32);
		bpb->activeFat = getI16(bpb32_raw->fatStatus);
		if (bpb->activeFat & 0x80) { //fat not synced
			bpb->activeFat = (bpb->activeFat & 0xF);
		} else {
			bpb->activeFat = 0;
		}
		bpb->rootDirStart = part_rec->start + (bpb->fatCount * bpb->fatSize) + bpb->resSectors;
		bpb->rootDirCluster = getI32(bpb32_raw->rootDirCluster);
		for (ret = 0; ret < 8; ret++) {
			bpb->fatId[ret] = bpb32_raw->fatId[ret];
		}
		bpb->fatId[ret] = 0;
	}
}

/*
 returns:
 0 - no more dir entries
 1 - short name dir entry found
 2 - long name dir entry found
 3 - deleted dir entry found
*/
int fat_getDirentry(fat_direntry_sfn* dsfn, fat_direntry_lfn* dlfn, fat_direntry* dir ) {
	int i, j;
	int offset;
	int cont;

	//detect last entry - all zeros (slight modification by radad)
	if (dsfn->name[0] == 0) {
		return 0;
	}
	//detect deleted entry - it will be ignored
	if (dsfn->name[0] == 0xE5) {
		return 3;
	}

	//detect long filename
	if (dlfn->rshv == 0x0F && dlfn->reserved1 == 0x00 && dlfn->reserved2[0] == 0x00) {
		//long filename - almost whole direntry is unicode string - extract it
		offset = dlfn->entrySeq & 0x3f;
		offset--;
		offset = offset * 13;
		//name - 1st part
		cont = 1;
		for (i = 0; i < 10 && cont; i+=2) {
			if (dlfn->name1[i]==0 && dlfn->name1[i+1] == 0) {
				dir->name[offset] = 0; //terminate
				cont = 0; //stop
			} else {
				dir->name[offset] = dlfn->name1[i];
				offset++;
			}
		}
		//name - 2nd part
		for (i = 0; i < 12 && cont; i+=2) {
			if (dlfn->name2[i]==0 && dlfn->name2[i+1] == 0) {
				dir->name[offset] = 0; //terminate
				cont = 0; //stop
			} else {
				dir->name[offset] = dlfn->name2[i];
				offset++;
			}
		}
		//name - 3rd part
		for (i = 0; i < 4 && cont; i+=2) {
			if (dlfn->name3[i]==0 && dlfn->name3[i+1] == 0) {
				dir->name[offset] = 0; //terminate
				cont = 0; //stop
			} else {
				dir->name[offset] = dlfn->name3[i];
				offset++;
			}
		}
		if ((dlfn->entrySeq & 0x40)) { //terminate string flag
			dir->name[offset] = 0;
		}
		return 2;
	} else {
		//short filename
		//copy name

		for (i = 0; i < 8 && dsfn->name[i]!= 32; i++) {
			dir->sname[i] = dsfn->name[i];
			// NT—adaption for LaunchELF
			if (dsfn->reservedNT & 0x08 &&
			  dir->sname[i] >= 'A' && dir->sname[i] <= 'Z') {
				dir->sname[i] += 0x20;	//Force standard letters in name to lower case
			}
		}
		for (j=0; j < 3 && dsfn->ext[j] != 32; j++) {
			if (j == 0) {
				dir->sname[i] = '.';
				i++;
			}
			dir->sname[i+j] = dsfn->ext[j];
			// NT—adaption for LaunchELF
			if (dsfn->reservedNT & 0x10 &&
			  dir->sname[i+j] >= 'A' && dir->sname[i+j] <= 'Z') {
				dir->sname[i+j] += 0x20;	//Force standard letters in ext to lower case
			}
		}
		dir->sname[i+j] = 0; //terminate
		if (dir->name[0] == 0) { //long name desn't exit
			for (i =0 ; dir->sname[i] !=0; i++) dir->name[i] = dir->sname[i];
			dir->name[i] = 0;
		}
		dir->attr = dsfn->attr;
		dir->size = getI32(dsfn->size);
		dir->cluster = getI32_2(dsfn->clusterL, dsfn->clusterH);
		return 1;
	}

}


//Set chain info (cluster/offset) cache
void fat_setFatDirChain(fat_bpb* bpb, fat_dir* fatDir) {
	int i,j;
	int index;
	int chainSize;
	int nextChain;
	int clusterChainStart ;
	unsigned int fileCluster;
	int fileSize;
	int blockSize;


	XPRINTF("FAT driver: reading cluster chain  \n");
	fileCluster = fatDir->chain[0].cluster;

	if (fileCluster < 2) {
		XPRINTF("   early exit... \n");
		return;
	}

	fileSize = fatDir->size;
	blockSize = fileSize / DIR_CHAIN_SIZE;

	nextChain = 1;
	clusterChainStart = 0;
	j = 1;
	fileSize = 0;
	index = 0;

	while (nextChain) {
		chainSize = fat_getClusterChain(bpb, fileCluster, cbuf, MAX_DIR_CLUSTER, 1);
		if (chainSize == 0) { //the chain is full, but more chain parts exist
			chainSize = MAX_DIR_CLUSTER;
			fileCluster = cbuf[MAX_DIR_CLUSTER - 1];
		}else { //chain fits in the chain buffer completely - no next chain exist
			nextChain = 0;
		}

		//process the cluster chain (cbuf)
		for (i = clusterChainStart; i < chainSize; i++) {
			fileSize += (bpb->clusterSize * bpb->sectorSize);
			while (fileSize >= (j * blockSize) && j < DIR_CHAIN_SIZE) {
				fatDir->chain[j].cluster = cbuf[i];
				fatDir->chain[j].index = index;
				j++;
			}
			index++;
		}
		clusterChainStart = 1;

	}
	fatDir->lastCluster = cbuf[i-1];

#ifdef DEBUG
	//debug
	printf("SEEK CLUSTER CHAIN CACHE fileSize=%i blockSize=%i \n", fatDir->size, blockSize);
	for (i = 0; i < DIR_CHAIN_SIZE; i++) {
		printf("index=%i cluster=%i offset= %i - %i start=%i \n",
			fatDir->chain[i].index, fatDir->chain[i].cluster,
			fatDir->chain[i].index * bpb->clusterSize * bpb->sectorSize,
			(fatDir->chain[i].index+1) * bpb->clusterSize * bpb->sectorSize,
			i * blockSize);
	}
#endif /* debug */
	XPRINTF("FAT driver: read cluster chain  done!\n");


}


/* Set base attributes of direntry */
void fat_setFatDir(fat_bpb* bpb, fat_dir* fatDir, fat_direntry_sfn* dsfn, fat_direntry* dir, int getClusterInfo ) {
	int i;
	unsigned char* srcName;

	XPRINTF("setting fat dir...\n");
	srcName = dir->sname;
	if (dir->name[0] != 0) { //long filename not empty
		srcName = dir->name;
	}
	//copy name
	for (i = 0; srcName[i] != 0; i++) fatDir->name[i] = srcName[i];
	fatDir->name[i] = 0; //terminate

	fatDir->attr = dsfn->attr;
	fatDir->size = dir->size;

	//created Date: Day, Month, Year-low, Year-high
	fatDir->cdate[0] = (dsfn->dateCreate[0] & 0x1F);
	fatDir->cdate[1] = (dsfn->dateCreate[0] >> 5) + ((dsfn->dateCreate[1] & 0x01) << 3 );
	i = 1980 + (dsfn->dateCreate[1] >> 1);
	fatDir->cdate[2] = (i & 0xFF);
	fatDir->cdate[3] = ((i & 0xFF00)>> 8);

	//created Time: Hours, Minutes, Seconds
	fatDir->ctime[0] = ((dsfn->timeCreate[1] & 0xF8) >> 3);
	fatDir->ctime[1] = ((dsfn->timeCreate[1] & 0x07) << 3) + ((dsfn->timeCreate[0] & 0xE0) >> 5);
	fatDir->ctime[6] = ((dsfn->timeCreate[0] & 0x1F) << 1);

	//accessed Date: Day, Month, Year-low, Year-high
	fatDir->adate[0] = (dsfn->dateAccess[0] & 0x1F);
	fatDir->adate[1] = (dsfn->dateAccess[0] >> 5) + ((dsfn->dateAccess[1] & 0x01) << 3 );
	i = 1980 + (dsfn->dateAccess[1] >> 1);
	fatDir->adate[2] = (i & 0xFF);
	fatDir->adate[3] = ((i & 0xFF00)>> 8);

	//modified Date: Day, Month, Year-low, Year-high
	fatDir->mdate[0] = (dsfn->dateWrite[0] & 0x1F);
	fatDir->mdate[1] = (dsfn->dateWrite[0] >> 5) + ((dsfn->dateWrite[1] & 0x01) << 3 );
	i = 1980 + (dsfn->dateWrite[1] >> 1);
	fatDir->mdate[2] = (i & 0xFF);
	fatDir->mdate[3] = ((i & 0xFF00)>> 8);

	//modified Time: Hours, Minutes, Seconds
	fatDir->mtime[0] = ((dsfn->timeWrite[1] & 0xF8) >> 3);
	fatDir->mtime[1] = ((dsfn->timeWrite[1] & 0x07) << 3) + ((dsfn->timeWrite[0] & 0xE0) >> 5);
	fatDir->mtime[2] = ((dsfn->timeWrite[0] & 0x1F) << 1);

	fatDir->chain[0].cluster = dir->cluster;
	fatDir->chain[0].index  = 0;
	if (getClusterInfo) {
		fat_setFatDirChain(bpb, fatDir);
	}
}


int fat_getDirentrySectorData(fat_bpb* bpb, unsigned int* startCluster, unsigned int* startSector, int* dirSector) {
	int chainSize;

	if (*startCluster == 0 && bpb->fatType < FAT32) { //Root directory
		*startSector = bpb->rootDirStart;
		*dirSector =  bpb->rootSize / (bpb->sectorSize / 32);
		return 0;
	}
	 //other directory or fat 32
	if (*startCluster == 0 && bpb->fatType == FAT32) {
		*startCluster = bpb->rootDirCluster;
	}
	*startSector = fat_cluster2sector(bpb, *startCluster);
	chainSize = fat_getClusterChain(bpb, *startCluster, cbuf, MAX_DIR_CLUSTER, 1);
	if (chainSize > 0) {
		*dirSector = chainSize * bpb->clusterSize;
	} else {
		printf("FAT driver: Error getting cluster chain! startCluster=%i \n", *startCluster);
		return -1;
	}

	return chainSize;
}

int fat_getDirentryStartCluster(fat_bpb* bpb, unsigned char* dirName, unsigned int* startCluster, fat_dir* fatDir) {
	fat_direntry_sfn* dsfn;
	fat_direntry_lfn* dlfn;
	fat_direntry dir;
	int i;
	int dirSector;
	unsigned int startSector;
	int cont;
	int ret;
	int dirPos;
	int clusterMod;

	cont = 1;
	XPRINTF("\n");
	XPRINTF("getting cluster for dir entry: %s \n", dirName);
	clusterMod = bpb->clusterSize - 1;
	//clear name strings
	dir.sname[0] = 0;
	dir.name[0] = 0;

	fat_getDirentrySectorData(bpb, startCluster, &startSector, &dirSector);

	XPRINTF("dirCluster=%i startSector=%i (%i) dirSector=%i \n", *startCluster, startSector, startSector * Size_Sector, dirSector);

	//go through first directory sector till the max number of directory sectors
	//or stop when no more direntries detected
	for (i = 0; i < dirSector && cont; i++) {
		ret = READ_SECTOR(startSector + i, sbuf);
		if (ret < 0) {
			printf("FAT driver: read directory sector failed ! sector=%i\n", startSector + i);
			return FAT_ERROR;
		}
		XPRINTF("read sector ok, scanning sector for direntries...\n");

		//get correct sector from cluster chain buffer
		if ((*startCluster != 0) && (i % bpb->clusterSize == clusterMod)) {
			startSector = fat_cluster2sector(bpb, cbuf[(i / bpb->clusterSize) +  1]);
			startSector -= (i+1);
		}
		dirPos = 0;

		// go through start of the sector till the end of sector
		while (cont &&  dirPos < bpb->sectorSize) {
			dsfn = (fat_direntry_sfn*) (sbuf + dirPos);
			dlfn = (fat_direntry_lfn*) (sbuf + dirPos);
			cont = fat_getDirentry(dsfn, dlfn, &dir); //get single directory entry from sector buffer
			if (cont == 1) { //when short file name entry detected
				if (!(dir.attr & 0x08)) { //not volume label
					if ((strEqual(dir.sname, dirName) == 0) ||
						(strEqual(dir.name, dirName) == 0) ) {
							XPRINTF("found! %s\n", dir.name);
							if (fatDir != NULL) { //fill the directory properties
								fat_setFatDir(bpb, fatDir, dsfn, &dir, 1);
							}
							*startCluster = dir.cluster;
							XPRINTF("direntry %s found at cluster: %i \n", dirName, dir.cluster);
							return dir.attr; //returns file or directory attr
						}
				}
				//clear name strings
				dir.sname[0] = 0;
				dir.name[0] = 0;
			}
			dirPos += 32; //directory entry of size 32 bytes
		}

	}
	XPRINTF("direntry %s not found! \n", dirName);
	return -EFAULT;
}

// start cluster should be 0 - if we want to search from root directory
// otherwise the start cluster should be correct cluster of directory
// to search directory - set fatDir as NULL
int fat_getFileStartCluster(fat_bpb* bpb, const char* fname, unsigned int* startCluster, fat_dir* fatDir) {
	unsigned char tmpName[257];
	int i;
	int offset;
	int cont;
	int ret;

	cont = 1;
	offset = 0;

	i=0;
	if (fname[i] == '/') {
		i++;
	}

	for ( ; fname[i] !=0; i++) {
		if (fname[i] == '/') { //directory separator
			tmpName[offset] = 0; //terminate string
			ret = fat_getDirentryStartCluster(bpb, tmpName, startCluster, fatDir);
			if (ret < 0) {
				return -ENOENT;
			}
			offset = 0;
		} else{
			tmpName[offset] = fname[i];
			offset++;
		}
	}
	//and the final file
	if (fatDir != NULL) {
		//if the last char of the name was slash - the name was already found -exit
		if (offset == 0 && i > 1) {
			return 1;
		}
		tmpName[offset] = 0; //terminate string
		ret = fat_getDirentryStartCluster(bpb, tmpName, startCluster, fatDir);
		if (ret < 0) {
			return ret;
		}
		XPRINTF("file's startCluster found. Name=%s, cluster=%i \n", fname, *startCluster);
	}
	return 1;
}

void fat_getClusterAtFilePos(fat_bpb* bpb, fat_dir* fatDir, unsigned int filePos, unsigned int* cluster, unsigned int* clusterPos) {
	int i;
	int blockSize;
	int j = (DIR_CHAIN_SIZE-1);

	blockSize = bpb->clusterSize * bpb->sectorSize;

	for (i = 0; i < (DIR_CHAIN_SIZE-1); i++) {
		if (fatDir->chain[i].index   * blockSize <= filePos &&
			fatDir->chain[i+1].index * blockSize >  filePos) {
				j = i;
				break;
			}
	}
	*cluster    = fatDir->chain[j].cluster;
	*clusterPos = (fatDir->chain[j].index * blockSize);
}

int fat_readFile(fat_bpb* bpb, fat_dir* fatDir, unsigned int filePos, unsigned char* buffer, int size) {
	int ret;
	int i,j;
	int chainSize;
	int nextChain;
	int startSector;
	int bufSize;
	int sectorSkip;
	int clusterSkip;
	int dataSkip;

	unsigned int bufferPos;
	unsigned int fileCluster;
	unsigned int clusterPos;

	int clusterChainStart;

	fat_getClusterAtFilePos(bpb, fatDir, filePos, &fileCluster, &clusterPos);
	sectorSkip = (filePos - clusterPos) / bpb->sectorSize;
	clusterSkip = sectorSkip / bpb->clusterSize;
	sectorSkip %= bpb->clusterSize;
	dataSkip  = filePos  % bpb->sectorSize;
	bufferPos = 0;

	XPRINTF("fileCluster = %i,  clusterPos= %i clusterSkip=%i, sectorSkip=%i dataSkip=%i \n",
		fileCluster, clusterPos, clusterSkip, sectorSkip, dataSkip);

	if (fileCluster < 2) {
		return 0;
	}

	bufSize = SECTOR_SIZE;
	nextChain = 1;
	clusterChainStart = 1;

	while (nextChain && size > 0 ) {
		chainSize = fat_getClusterChain(bpb, fileCluster, cbuf, MAX_DIR_CLUSTER, clusterChainStart);
		clusterChainStart = 0;
		if (chainSize == 0) { //the chain is full, but more chain parts exist
			chainSize = MAX_DIR_CLUSTER;
			fileCluster = cbuf[MAX_DIR_CLUSTER - 1];
		}else { //chain fits in the chain buffer completely - no next chain needed
			nextChain = 0;
		}
		while (clusterSkip >= MAX_DIR_CLUSTER) {
			chainSize = fat_getClusterChain(bpb, fileCluster, cbuf, MAX_DIR_CLUSTER, clusterChainStart);
			clusterChainStart = 0;
			if (chainSize == 0) { //the chain is full, but more chain parts exist
				chainSize = MAX_DIR_CLUSTER;
				fileCluster = cbuf[MAX_DIR_CLUSTER - 1];
			}else { //chain fits in the chain buffer completely - no next chain needed
				nextChain = 0;
			}
			clusterSkip -= MAX_DIR_CLUSTER;
		}

		//process the cluster chain (cbuf) and skip leading clusters if needed
		for (i = 0 + clusterSkip; i < chainSize && size > 0; i++) {
			//read cluster and save cluster content
			startSector = fat_cluster2sector(bpb, cbuf[i]);
			//process all sectors of the cluster (and skip leading sectors if needed)
			for (j = 0 + sectorSkip; j < bpb->clusterSize && size > 0; j++) {
				ret = READ_SECTOR(startSector + j, sbuf);
				if (ret < 0) {
					printf("Read sector failed ! sector=%i\n", startSector + j);
					return bufferPos;
				}

				//compute exact size of transfered bytes
				if (size < bufSize) {
					bufSize = size + dataSkip;
				}
				if (bufSize > SECTOR_SIZE) {
					bufSize = SECTOR_SIZE;
				}
				XPRINTF("memcopy dst=%i, src=%i, size=%i  bufSize=%i \n", bufferPos, dataSkip, bufSize-dataSkip, bufSize);
				MEMCPY(buffer+bufferPos, sbuf + dataSkip, bufSize - dataSkip);
				size-= (bufSize - dataSkip);
				bufferPos +=  (bufSize - dataSkip);
				dataSkip = 0;
				bufSize = SECTOR_SIZE;
			}
			sectorSkip = 0;
		}
		clusterSkip = 0;
	}
	return bufferPos;
}

extern mass_dev mass_device;

int fat_mountCheck()
{
	int mediaStatus;
	int ret;

	mediaStatus = mass_stor_getStatus(&mass_device);
	if (mediaStatus < 0)
	{
		mounted = 0;
		return mediaStatus;
	}
	if ((mediaStatus & 0x03) == 3)
	{ /* media is ready for operation */
		/* in the meantime the media was reconnected and maybe changed - force unmount*/
		if((mediaStatus & 0x04) == 4) { mounted = 0; }
		else if (mounted) { return 1; }
        ret = fat_initDriver();
		return ret;
	}

	/* fs mounted but media is not ready - force unmount */
	if (mounted) { mounted = 0; }
	return -10;
}

int fat_getNextDirentry(fat_dir* fatDir) {
	fat_bpb* bpb;
	fat_direntry_sfn* dsfn;
	fat_direntry_lfn* dlfn;
	fat_direntry dir;
	int i;
	int dirSector;
	unsigned int startSector;
	int cont = 1;
	int ret;
	int dirPos;
	int clusterMod;
	int index;
	unsigned int dirCluster;

	//the getFirst function was not called
	if (direntryCluster == 0xFFFFFFFF || fatDir == NULL) {
		return -2;
	}
	bpb = &partBpb;

	dirCluster = direntryCluster;
	index  = 0;

	clusterMod = bpb->clusterSize - 1;

	//clear name strings
	dir.sname[0] = 0;
	dir.name[0] = 0;

	fat_getDirentrySectorData(bpb, &dirCluster, &startSector, &dirSector);

	XPRINTF("dirCluster=%i startSector=%i (%i) dirSector=%i \n", dirCluster, startSector, startSector * Size_Sector, dirSector);

	//go through first directory sector till the max number of directory sectors
	//or stop when no more direntries detected
	for (i = 0; i < dirSector && cont; i++) {
		ret = READ_SECTOR(startSector + i, sbuf);
		if (ret < 0) {
			printf("Read directory  sector failed ! sector=%i\n", startSector + i);
			return -3;
		}
		//get correct sector from cluster chain buffer
		if ((dirCluster != 0) && (i % bpb->clusterSize == clusterMod)) {
			startSector = fat_cluster2sector(bpb, cbuf[(i / bpb->clusterSize) +  1]);
			startSector -= (i+1);
		}
		dirPos = 0;

		// go through start of the sector till the end of sector
		while (cont &&  dirPos < bpb->sectorSize) {
			dsfn = (fat_direntry_sfn*) (sbuf + dirPos);
			dlfn = (fat_direntry_lfn*) (sbuf + dirPos);
			cont = fat_getDirentry(dsfn, dlfn, &dir); //get single directory entry from sector buffer
			if (cont == 1) { //when short file name entry detected
				index++;
				if ((index-1) == direntryIndex) {
						direntryIndex++;
						fat_setFatDir(bpb, fatDir, dsfn, &dir, 0);
						return 1;
				}
				//clear name strings
				dir.sname[0] = 0;
				dir.name[0] = 0;
			}
			dirPos += 32; //directory entry of size 32 bytes
		}

	}
	// when we get this far - reset the direntry cluster
	direntryCluster = 0xFFFFFFFF; //no more files
	return -1; //indicate that no direntry is avalable
}

int fat_getFirstDirentry(char * dirName, fat_dir* fatDir) {
	int ret;
	unsigned int startCluster = 0;

	ret = fat_mountCheck();
	if (ret < 0) {
		return ret;
	}
   if ((dirName == NULL) || (dirName[0] == 0) ||
      (dirName[0] == '.') ||
      ((dirName[0] == '/' || dirName[0]=='\\') && ((dirName[1] == 0) || (dirName[1] == '.')))) // the root directory
      {
			direntryCluster = 0;
	} else {
		ret = fat_getFileStartCluster(&partBpb, dirName, &startCluster, fatDir);
		if (ret < 0) { //dir name not found
			return -4;
		}
		//check that direntry is directory
		if ((fatDir->attr & 0x10) == 0) {
			return -3; //it's a file - exit
		}
		direntryCluster = startCluster;
	}
	direntryIndex = 0;
	return fat_getNextDirentry(fatDir);
}


int fat_initDriver() {
	int ret = 0;

    mounted = 0;

	lastChainCluster = 0xFFFFFFFF;
	lastChainResult = -1;

	direntryCluster = 0xFFFFFFFF;
	direntryIndex = 0;

	ret = DISK_INIT("disk1.bin", Size_Sector); // modified by Hermes
	if (ret < 0) {
		printf ("fat_driver: disk init failed \n" );
		return ret;
	}
	fat_getPartitionTable(&partTable);
	fat_getPartitionBootSector(&partTable.record[workPartition],  &partBpb);

	mounted = 1;
    return(ret);
}

void fat_closeDriver() {
	DISK_CLOSE();
}

fat_bpb * fat_getBpb() {
 	return &partBpb;
}


int fat_readSector(unsigned int sector, unsigned char** buf)
{
	int ret;

	ret = READ_SECTOR(sector, sbuf);
	if (ret < 0) {
		printf("Read sector failed ! sector=%i\n", sector);
		return -1;
	}
	*buf = sbuf;
	return Size_Sector;
}
