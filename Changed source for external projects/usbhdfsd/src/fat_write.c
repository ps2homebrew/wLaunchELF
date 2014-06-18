/*
 * fat_driver.c - USB Mass storage driver for PS2
 *
 * (C) 2005, Marek Olejnik (ole00@post.cz)
 *
 * FAT filesystem layer -  write functions
 *
 * See the file LICENSE included with this distribution for licensing terms.
 */

#include <tamtypes.h>
#include <cdvdman.h>
#include <sysclib.h>
#include <stdio.h>

#include <errno.h>

#include "fat.h"
#include "fat_driver.h"
#include "scache.h"
#include "usbhd_common.h"

#include "mass_debug.h"

#define DATE_CREATE 1
#define DATE_MODIFY 2

#define READ_SECTOR(a, b)	scache_readSector((a), (void **)&b)
#define ALLOC_SECTOR(a, b)	scache_allocSector((a), (void **)&b)
#define WRITE_SECTOR(a)		scache_writeSector((a))
#define FLUSH_SECTORS		scache_flushSectors

#define DIRENTRY_COUNT 1024
#define MAX_CLUSTER_STACK 128

#define MEMCPY(a,b,c) memcpy((a),(b),(c))

#define SECTOR_SIZE Size_Sector
extern unsigned Size_Sector; // store size of sector from usb mass

extern unsigned char* sbuf; //sector buffer
extern unsigned int cbuf[MAX_DIR_CLUSTER]; //cluster index buffer
unsigned char tbuf[512]; //temporary buffer

/* enough for long filename of length 260 characters (20*13) and one short filename */
#define MAX_DE_STACK 21
unsigned int deSec[MAX_DE_STACK]; //direntry sector
int          deOfs[MAX_DE_STACK]; //direntry offset
int          deIdx = 0; //direntry index


unsigned int clStack[MAX_CLUSTER_STACK]; //cluster allocation stack
int clStackIndex = 0;
unsigned int clStackLast = 0; // last free cluster of the fat table

/*
 reorder (swap) the cluster stack records
*/
void swapClStack(int startIndex, int endIndex) {
	int i;
	int size;
	int offset1, offset2;
	unsigned int tmp;

	size = endIndex-startIndex;
	if (size < 2) {
		return;
	}

	size/=2;
	for (i = 0; i < size; i++) {
		offset1 = startIndex + i;
		offset2 = endIndex - 1 - i;
		tmp = clStack[offset1];
		clStack[offset1] = clStack[offset2];
		clStack[offset2] = tmp;
	}
}

/*
 scan FAT12 for free clusters and store them to the cluster stack
*/

int fat_readEmptyClusters12(fat_bpb* bpb) {
	int ret;
	int i;
	int recordOffset;
	int sectorSpan;
	int fatSector;
	int cont;
	int lastFatSector;
	unsigned int cluster;
	unsigned int clusterValue;
	unsigned char xbuf[4];
	int oldClStackIndex;

	oldClStackIndex = clStackIndex;

	cont = 1;
	lastFatSector = -1;
	i = 0;
	cluster = clStackLast;

	while(clStackIndex < MAX_CLUSTER_STACK ) {
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
					return -EIO;
				}
				lastFatSector = fatSector;

				if (sectorSpan) {
					xbuf[0] = sbuf[bpb->sectorSize - 2];
					xbuf[1] = sbuf[bpb->sectorSize - 1];
					ret = READ_SECTOR(bpb->partStart + bpb->resSectors + fatSector + 1, sbuf);
					if (ret < 0) {
						printf("FAT driver:Read fat12 sector failed sector=%i! \n", bpb->partStart + bpb->resSectors + fatSector + 1);
						return -EIO;
					}
					xbuf[2] = sbuf[0];
					xbuf[3] = sbuf[1];
				}
		}
		if (sectorSpan) { // use xbuf as source buffer
			clusterValue = fat_getClusterRecord12(xbuf + (recordOffset % bpb->sectorSize) - (bpb->sectorSize-2), cluster % 2);
		} else { // use sector buffer as source buffer
			clusterValue = fat_getClusterRecord12(sbuf + (recordOffset % bpb->sectorSize), cluster % 2);
		}
		if (clusterValue == 0) {
			clStackLast = cluster;
			clStack[clStackIndex] = cluster;
			clStackIndex++;

		}
		cluster++; //read next cluster record in the sequence
	}
	//the stack operates as LIFO but we put in the clusters as FIFO
	//we should reverse the cluster order - not necessary
	//but it will retain the natural (increasing) order of
	//the cluster chain
	swapClStack(oldClStackIndex, clStackIndex);
	return clStackIndex;

}


/*
 scan FAT32 for free clusters and store them to the cluster stack
*/
int fat_readEmptyClusters32(fat_bpb* bpb) {
        int i,j;
	int ret;
	int indexCount;
	unsigned int fatStartSector;
	unsigned int cluster;
	unsigned int clusterValue;
	int oldClStackIndex;
	int sectorSkip;
	int recordSkip;

	oldClStackIndex = clStackIndex;

	//indexCount = numer of cluster indices per sector
	indexCount = bpb->sectorSize / 4; //FAT16->2, FAT32->4
	//skip areas we have already searched through
	sectorSkip = clStackLast / indexCount;
	recordSkip = clStackLast % indexCount;

	fatStartSector = bpb->partStart + bpb->resSectors;

	for (i = sectorSkip; i < bpb->fatSize && clStackIndex < MAX_CLUSTER_STACK ; i++) {
		ret = READ_SECTOR(fatStartSector + i,  sbuf);
		if (ret < 0) {
			printf("FAT driver:Read fat32 sector failed! sector=%i! \n", fatStartSector + i);
			return -EIO;
		}
		for (j = recordSkip; j < indexCount && clStackIndex < MAX_CLUSTER_STACK ; j++) {
			cluster = getI32(sbuf + (j * 4));
			if (cluster == 0) { //the cluster is free
				clusterValue = (i * indexCount) + j;
				if (clusterValue < 0xFFFFFF7) {
					clStackLast = clusterValue;
					clStack[clStackIndex] = clusterValue;
					clStackIndex++;
				}
			}
		}
		recordSkip = 0;
	}
	//the stack operates as LIFO but we put in the clusters as FIFO
	//we should reverse the cluster order - not necessary
	//but it will retain the natural (increasing) order of
	//the cluster chain
	swapClStack(oldClStackIndex, clStackIndex);
	return clStackIndex;
}

/*
 scan FAT16 for free clusters and store them to the cluster stack
*/
int fat_readEmptyClusters16(fat_bpb* bpb) {
        int i,j;
	int ret;
	int indexCount;
	unsigned int fatStartSector;
	unsigned int cluster;
	int oldClStackIndex;
	int sectorSkip;
	int recordSkip;

	oldClStackIndex = clStackIndex;
	//XPRINTF("#### Read empty clusters16: clStackIndex=%d MAX=%d\n",  clStackIndex, MAX_CLUSTER_STACK);

	//indexCount = numer of cluster indices per sector
	indexCount = bpb->sectorSize / 2; //FAT16->2, FAT32->4

	//skip areas we have already searched through
	sectorSkip = clStackLast / indexCount;
	recordSkip = clStackLast % indexCount;

	fatStartSector = bpb->partStart + bpb->resSectors;

	for (i = sectorSkip; i < bpb->fatSize && clStackIndex < MAX_CLUSTER_STACK ; i++) {
		ret = READ_SECTOR(fatStartSector + i,  sbuf);
		if (ret < 0) {
			printf("FAT driver:Read fat16 sector failed! sector=%i! \n", fatStartSector + i);
			return -EIO;
		}
		for (j = recordSkip; j < indexCount && clStackIndex < MAX_CLUSTER_STACK ; j++) {
			cluster = getI16(sbuf + (j * 2));
			if (cluster == 0) { //the cluster is free
				clStackLast = (i * indexCount) + j;
				clStack[clStackIndex] = clStackLast;
				XPRINTF("%d ", clStack[clStackIndex]);
				clStackIndex++;
			}
		}
		recordSkip = 0;
	}
	XPRINTF("\n");
	//the stack operates as LIFO but we put in the clusters as FIFO
	//we should reverse the cluster order - not necessary
	//but it will retain the natural (increasing) order of
	//the cluster chain
	swapClStack(oldClStackIndex, clStackIndex);
	return clStackIndex;
}


/*
 scan FAT for free clusters and store them to the cluster stack
*/
int fat_readEmptyClusters(fat_bpb* bpb) {
	switch (bpb->fatType)
	{
		case FAT12: return fat_readEmptyClusters12(bpb);
		case FAT16: return fat_readEmptyClusters16(bpb);
		case FAT32: return fat_readEmptyClusters32(bpb);
	}

    return(-1);
}



/*
   set sinlge cluster record (FAT12)into buffer

   0x321, 0xABC

    byte0|byte1|byte2|
   +--+--+--+--+--+--+
   |2 |1 |C |3 |A |B |
   +--+--+--+--+--+--+

*/
void fat_setClusterRecord12(unsigned char* buf, unsigned int cluster, int type) {

	if (type) { //type 1
		buf[0] = (buf[0] & 0x0F) + ((cluster & 0x0F)<<4) ;
		buf[1] = (cluster & 0xFF0) >> 4;
	} else { // type 0
		buf[0] = (cluster & 0xFF);
		buf[1] = (buf[1] & 0xF0) + ((cluster & 0xF00) >> 8);
	}

}

void fat_setClusterRecord12part1(unsigned char* buf, unsigned int cluster, int type) {
	if (type) { //type 1
		buf[0] = (buf[0] & 0x0F) + ((cluster & 0x0F)<<4);
	} else { // type 0
		buf[0] = (cluster & 0xFF);
	}
}

void fat_setClusterRecord12part2(unsigned char* buf, unsigned int cluster, int type) {
	if (type) { //type 1
		buf[0] = (cluster & 0xFF0) >> 4;
	} else { // type 0
		buf[0] = (buf[0] & 0xF0) + ((cluster & 0xF00) >> 8);
	}
}

/*
   save value at the cluster record in FAT 12
*/
int fat_saveClusterRecord12(fat_bpb* bpb, unsigned int currentCluster, unsigned int value)
{
	int ret;
	int sectorSpan;
	int recordOffset;
	int recordType;
	int fatNumber;
	unsigned int fatSector;

	ret = -1;
	//recordOffset is byte offset of the record from the start of the fat table
	recordOffset =  (currentCluster * 3) / 2;

	//save both fat tables
	for (fatNumber = 0; fatNumber < bpb->fatCount; fatNumber++) {

		fatSector  = bpb->partStart + bpb->resSectors + (fatNumber * bpb->fatSize);
		fatSector += recordOffset / bpb->sectorSize;
		sectorSpan = bpb->sectorSize - (recordOffset % bpb->sectorSize);
		if (sectorSpan > 1) {
			sectorSpan = 0;
		}
		recordType = currentCluster % 2;

		ret = READ_SECTOR(fatSector,  sbuf);
		if (ret < 0) {
			printf("FAT driver:Read fat16 sector failed! sector=%i! \n", fatSector);
			return -EIO;
		}
		if (!sectorSpan) { // not sector span - the record is copmact and fits in single sector
	 		fat_setClusterRecord12(sbuf + (recordOffset % bpb->sectorSize), value, recordType);
			ret = WRITE_SECTOR(fatSector);
			if (ret < 0) {
				printf("FAT driver:Write fat12 sector failed! sector=%i! \n", fatSector);
				return -EIO;
			}
		} else { // sector span - the record is broken in 2 pieces - each one on different sector
			//modify one last byte of the sector buffer
			fat_setClusterRecord12part1(sbuf + (recordOffset % bpb->sectorSize), value, recordType);
			//save current sector
			ret = WRITE_SECTOR(fatSector);
			if (ret < 0) {
				printf("FAT driver:Write fat12 sector failed! sector=%i! \n", fatSector);
				return -EIO;
			}
			//read next sector from the fat
			fatSector++;
			ret = READ_SECTOR(fatSector,  sbuf);
			if (ret < 0) {
				printf("FAT driver:Read fat16 sector failed! sector=%i! \n", fatSector);
				return -EIO;
			}
			//modify first byte of the sector buffer
			fat_setClusterRecord12part2(sbuf, value, recordType);
			//save current sector
			ret = WRITE_SECTOR(fatSector);
			if (ret < 0) {
				printf("FAT driver:Write fat12 sector failed! sector=%i! \n", fatSector);
				return -EIO;
			}
		}
	} //end for
	return ret;
}

/*
   save value at the cluster record in FAT 32
*/
int fat_saveClusterRecord16(fat_bpb* bpb, unsigned int currentCluster, unsigned int value) {
        int i;
	int ret;
	int indexCount;
	int fatNumber;
	unsigned int fatSector;

	ret = -1;
	//indexCount is numer of cluster indices per sector
	indexCount = bpb->sectorSize / 2; //FAT16->2, FAT32->4

	//save both fat tables
	for (fatNumber = 0; fatNumber < bpb->fatCount; fatNumber++) {
		fatSector  = bpb->partStart + bpb->resSectors + (fatNumber * bpb->fatSize);
		fatSector += currentCluster / indexCount;

		ret = READ_SECTOR(fatSector,  sbuf);
		if (ret < 0) {
			printf("FAT driver:Read fat16 sector failed! sector=%i! \n", fatSector);
			return -EIO;
		}
		i = currentCluster % indexCount;
		i*=2; //fat16
		sbuf[i++] = value & 0xFF;
		sbuf[i]   = ((value & 0xFF00) >> 8);
		ret = WRITE_SECTOR(fatSector);
		if (ret < 0) {
			printf("FAT driver:Writed fat16 sector failed! sector=%i! \n", fatSector);
			return -EIO;
		}
	}
	return ret;
}

/*
   save value at the cluster record in FAT 16
*/
int fat_saveClusterRecord32(fat_bpb* bpb, unsigned int currentCluster, unsigned int value) {
        int i;
	int ret;
	int indexCount;
	int fatNumber;
	unsigned int fatSector;

	ret = -1;
	//indexCount is numer of cluster indices per sector
	indexCount = bpb->sectorSize / 4; //FAT16->2, FAT32->4

	//save both fat tables
	for (fatNumber = 0; fatNumber < bpb->fatCount; fatNumber++) {
		fatSector  = bpb->partStart + bpb->resSectors + (fatNumber * bpb->fatSize);
		fatSector += currentCluster / indexCount;

		ret = READ_SECTOR(fatSector,  sbuf);
		if (ret < 0) {
			printf("FAT driver:Read fat32 sector failed! sector=%i! \n", fatSector);
			return -EIO;
		}
		i = currentCluster % indexCount;
		i*=4; //fat32
		sbuf[i++] = value & 0xFF;
		sbuf[i++] = ((value & 0xFF00) >> 8);
		sbuf[i++] = ((value & 0xFF0000) >> 16);
		sbuf[i]   = (sbuf[i] &0xF0) + ((value >> 24) & 0x0F); //preserve the highest nibble intact

		ret = WRITE_SECTOR(fatSector);
		if (ret < 0) {
			printf("FAT driver:Writed fat32 sector failed! sector=%i! \n", fatSector);
			return -EIO;
		}
	}
	return ret;
}


/*
  Append (and write) cluster chain to the FAT table.

  currentCluster - current end cluster record.
  endCluster     - new end cluster record.

  Note: there is no checking wether the currentCluster holds the EOF marker!

  Example
  current FAT:

  index   09    10    11    12
        +-----+-----+-----+-----+
  value + 10  | EOF | 0   | 0   |
        +-----+-----+-----+-----+


  currentcluster = 10, endcluster = 12
  updated FAT (after the function ends):

  index   09    10    11    12
        +-----+-----+-----+-----+
  value + 10  | 12  | 0   | EOF |
        +-----+-----+-----+-----+

*/

int fat_appendClusterChain(fat_bpb* bpb, unsigned int currentCluster, unsigned int endCluster) {
	int ret;
	ret = -1;
	switch (bpb->fatType) {
		case FAT12:
			ret = fat_saveClusterRecord12(bpb, currentCluster, endCluster);
			if (ret < 0) return ret;
			ret = fat_saveClusterRecord12(bpb, endCluster, 0xFFF);
			break;

		case FAT16:
			XPRINTF("I: appending cluster chain : current=%d end=%d \n", currentCluster, endCluster);
			ret = fat_saveClusterRecord16(bpb, currentCluster, endCluster);
			if (ret < 0) return ret;
			ret = fat_saveClusterRecord16(bpb, endCluster, 0xFFFF);
			break;

		case FAT32:
			ret = fat_saveClusterRecord32(bpb, currentCluster, endCluster);
			if (ret < 0) return ret;
			ret = fat_saveClusterRecord32(bpb, endCluster, 0xFFFFFFF);
			break;
	}
	return ret;
}

/*
 create new cluster chain (of size 1 cluster) at the cluster index
*/
int fat_createClusterChain(fat_bpb* bpb, unsigned int cluster) {
	switch (bpb->fatType) {
		case FAT12: return fat_saveClusterRecord12(bpb, cluster, 0xFFF);
		case FAT16: return fat_saveClusterRecord16(bpb, cluster, 0xFFFF);
		case FAT32: return fat_saveClusterRecord32(bpb, cluster, 0xFFFFFFF);
	}
	return -EFAULT;
}


/*
 modify the cluster (in FAT table) at the cluster index
*/
int fat_modifyClusterChain(fat_bpb* bpb, unsigned int cluster, unsigned int value) {
	switch (bpb->fatType) {
		case FAT12: return fat_saveClusterRecord12(bpb, cluster, value);
		case FAT16: return fat_saveClusterRecord16(bpb, cluster, value);
		case FAT32: return fat_saveClusterRecord32(bpb, cluster, value);
	}
	return -EFAULT;
}



/*
  delete cluster chain starting at cluster
*/
int fat_deleteClusterChain(fat_bpb* bpb, unsigned int cluster) {
	int ret;
	int size;
	int cont;
	int end;
	int i;

	if (cluster < 2) {
		return -EFAULT;
	}
	XPRINTF("I: delete cluster chain starting at cluster=%d\n", cluster);

	cont = 1;

	while (cont) {
		size = fat_getClusterChain(bpb, cluster, cbuf, MAX_DIR_CLUSTER, 1);
		if (size == 0) {
			size = MAX_DIR_CLUSTER;
		}

		end = size-1; //do not delete last cluster in the chain buffer

		for (i = 0 ; i < end; i++) {
			ret = fat_modifyClusterChain(bpb, cbuf[i], 0);
			if (ret < 0) {
				return ret;
			}
		}
		//the cluster chain continues
		if (size == MAX_DIR_CLUSTER) {
			cluster = cbuf[end];
		} else {
		//no more cluster entries - delete the last cluster entry
			ret = fat_modifyClusterChain(bpb, cbuf[end], 0);
			if (ret < 0) {
				return ret;
			}
			cont = 0;
		}
		fat_invalidateLastChainResult(); //prevent to misuse current (now deleted) cbuf
	}
	return 1;
}

/*
  Get single empty cluster from the clusterStack (cS is small cache of free clusters)
  Passed currentCluster is updated in the FAT and the new returned cluster index is
  appended at the end of fat chain!
*/
unsigned int fat_getFreeCluster(fat_bpb* bpb, unsigned int currentCluster) {

	int ret;
	unsigned int result;

	//cluster stack is empty - find and fill the cS
	if (clStackIndex <= 0) {
		clStackIndex = 0;
		ret = fat_readEmptyClusters(bpb);
		if (ret <= 0) return 0;
		clStackIndex = ret;

	}
	//pop from cluster stack
	clStackIndex--;
	result = clStack[clStackIndex];
        //append the cluster chain
	if (currentCluster) {
		ret = fat_appendClusterChain(bpb, currentCluster, result);
	} else { //create new cluster chain
		ret = fat_createClusterChain(bpb, result);
	}
	if (ret < 0) return 0;
	return result;
}


/*
 simple conversion of the char from lower case to upper case
*/
inline unsigned char toUpperChar(unsigned char c) {
	if (c >96  && c < 123) {
		return (c - 32);
	}
	return c;
}

/*
returns number of direntry positions that the name takes
*/
int getDirentrySize(unsigned char* lname) {
	int len;
	int result;
	len = strlen((const char*)lname);
	result = len / 13;
	if (len % 13 > 0) result++;
	return result;
}

/*
compute checksum of the short filename
*/
unsigned char computeNameChecksum(unsigned char* sname) {
	unsigned char result;
	int i;

	result = 0;
	for (i = 0; i < 11; i++) {
		result = (0x80 * (0x01 & result)) + (result >> 1);  //ROR 1
		result += sname[i];
	}
	return result;
}


/*
  fill the LFN (long filename) direntry
*/
void setLfnEntry(unsigned char* lname, int nameSize, unsigned char chsum, unsigned char* buffer, int part, int maxPart){
        int i,j;
	unsigned char* offset;
	unsigned char name[26]; //unicode name buffer = 13 characters per 2 bytes
	int nameStart;
	fat_direntry_lfn* dlfn;

        offset = buffer + (maxPart-part) * 32;
	dlfn = (fat_direntry_lfn*) offset;

	nameStart = 13 * part;
        j = nameSize - nameStart;
        if (j > 13) {
        	j = 13;
        }

        //fake unicode conversion
        for (i = 0; i < j; i++) {
        	name[i*2]   = lname[nameStart + i];
        	name[i*2+1] = 0;
        }

        //rest of the name is zero terminated and padded with 0xFF
        for (i = j; i < 13; i++) {
		if (i == j) {
	        	name[i*2]   = 0;
       			name[i*2+1] = 0;
		} else {
			name[i*2] = 0xFF;
			name[i*2+1] = 0xFF;
		}
        }

	if (maxPart == 0) {
		i = 1;
	} else {
		i = part/maxPart;
	}
	dlfn->entrySeq = (part+1) + (i * 64);
	dlfn->checksum = chsum;
	//1st part of the name
	for (i = 0; i < 10; i++) dlfn->name1[i] = name[i];
	//2nd part of the name
	for (i = 0; i < 12; i++) dlfn->name2[i] = name[i+10];
	//3rd part of the name
	for (i = 0; i < 4; i++) dlfn->name3[i] = name[i+22];
	dlfn->rshv = 0x0f;
	dlfn->reserved1 = 0;
	dlfn->reserved2[0] = 0;
	dlfn->reserved2[1] = 0;
}

/*
  update the SFN (long filename) direntry - DATE and TIME
*/
void setSfnDate(fat_direntry_sfn* dsfn, int mode) {
	int year, month, day, hour, minute, sec;
	unsigned char tmpClk[4];

	//ps2 specific routine to get time and date
	cd_clock_t	cdtime;
	s32		tmp;

	if(CdReadClock(&cdtime)!=0 && cdtime.stat==0)
	{

		tmp=cdtime.second>>4;
		sec=(u32)(((tmp<<2)+tmp)<<1)+(cdtime.second&0x0F);

		tmp=cdtime.minute>>4;
		minute=(((tmp<<2)+tmp)<<1)+(cdtime.minute&0x0F);

		tmp=cdtime.hour>>4;
		hour=(((tmp<<2)+tmp)<<1)+(cdtime.hour&0x0F);
		//hour= (hour + 4 + 12) % 24; // TEMP FIX (need to deal with timezones?)

		tmp=cdtime.day>>4;
		day=(((tmp<<2)+tmp)<<1)+(cdtime.day&0x0F);

		tmp=cdtime.month>>4;
		month=(((tmp<<2)+tmp)<<1)+(cdtime.month&0x0F);

		tmp=cdtime.year>>4;
		year=(((tmp<<2)+tmp)<<1)+(cdtime.year&0xF)+2000;
	} else  {
		year = 2005; month = 1;	day = 6;
		hour = 14; minute = 12; sec = 10;
	}

	if (dsfn == NULL || mode == 0)  {
		return;
	}

	tmpClk[0]  = (sec / 2) & 0x1F; //seconds
	tmpClk[0] += (minute & 0x07) << 5; // minute
	tmpClk[1]  = (minute & 0x38) >> 3; // minute
	tmpClk[1] += (hour & 0x1F) << 3; // hour

	tmpClk[2]  = (day & 0x1F); //day
	tmpClk[2] += (month & 0x07) << 5; // month
	tmpClk[3]  = (month & 0x08) >> 3; // month
	tmpClk[3] += ((year-1980) & 0x7F) << 1; //year

	XPRINTF("year=%d, month=%d, day=%d   h=%d m=%d s=%d \n", year, month, day, hour, minute, sec);
	//set date & time of creation
	if (mode & DATE_CREATE) {
		dsfn->timeCreate[0] = tmpClk[0];
		dsfn->timeCreate[1] = tmpClk[1];
		dsfn->dateCreate[0] = tmpClk[2];
		dsfn->dateCreate[1] = tmpClk[3];
		dsfn->dateAccess[0] = tmpClk[2];
		dsfn->dateAccess[1] = tmpClk[3];
	}
	//set  date & time of modification
	if (mode & DATE_MODIFY) {
		dsfn->timeWrite[0] = tmpClk[0];
		dsfn->timeWrite[1] = tmpClk[1];
		dsfn->dateWrite[0] = tmpClk[2];
		dsfn->dateWrite[1] = tmpClk[3];
	}
}

/*
  fill the SFN (short filename) direntry
*/
void setSfnEntry(unsigned char* shortName, char directory, unsigned char *buffer, unsigned int cluster) {
    int i;
	fat_direntry_sfn* dsfn;

	dsfn = (fat_direntry_sfn*) buffer;
	//name + ext
	for (i = 0; i < 8; i++) dsfn->name[i] = shortName[i];
	for (i = 0; i < 3; i++) dsfn->ext[i]  = shortName[i+8];

	if (directory > 0) {
		dsfn->attr = 16; //directory bit set
	} else {
		dsfn->attr = 32; //ordinary file + archive bit set
	}
	dsfn->reservedNT = 0;
	dsfn->clusterH[0] = (cluster & 0xFF0000) >> 16;
	dsfn->clusterH[1] = (cluster & 0xFF000000) >> 24;
	dsfn->clusterL[0] = (cluster & 0x00FF);
	dsfn->clusterL[1] = (cluster & 0xFF00) >> 8;

	//size is zero - because we don't know the filesize yet
	for (i = 0; i < 4; i++) dsfn->size[i] = 0;

	setSfnDate(dsfn, DATE_CREATE + DATE_MODIFY);
}




/*
 Create short name by squeezing long name into the 8.3 name boundaries
 lname - existing long name
 sname - buffer where to store short name

 returns: 0  if longname completely fits into the 8.3 boundaries
          1  if long name have to be truncated (ie. INFORM~1.TXT)
	 <0  if invalid long name detected
*/
int createShortNameMask(unsigned char* lname, unsigned char* sname) {
	int i;
	int size;
	int j;
	int fit;

	if (lname[0] == '.') {
		return -EINVAL;
	}

	fit = 0;
	//clean short name by putting space
	for (i = 0; i < 11; i++)  sname[i] = 32;
	XPRINTF("Clear short name ='%s'\n", sname);

	//detect number of dots and space characters in the long name
	j = 0;
	for (i = 0; lname[i] != 0; i++) {
		if (lname[i] == '.') j++; else
		if (lname[i] == 32 ) j+=2;
	}
	//long name contains no dot or one dot and no space char
	if (j <= 1) fit++;
	//XPRINTF("fit1=%d j=%d\n", fit, j);

	//store name
	for (i = 0; lname[i] !=0 && lname[i] != '.' && i < 8; i++) {
		sname[i] = toUpperChar(lname[i]);
		//short name must not contain spaces - replace space by underscore
		if (sname[i] == 32) sname[i]='_';
	}
	//check wether last char is '.' and the name is shorter than 8
	if (lname[i] == '.' || lname[i] == 0) {
		fit++;
	}
	//XPRINTF("fit2=%d\n", fit);

	//find the last dot "." - filename extension
	size = strlen((const char*)lname);
	size--;

	for (i = size; i > 0 && lname[i] !='.'; i--);
	if (lname[i] == '.') {
		i++;
		for (j=0; lname[i] != 0 && j < 3; i++, j++) {
			sname[j+8] = toUpperChar(lname[i]);
		}
		//no more than 3 characters of the extension
		if (lname[i] == 0) fit++;
	} else {
		//no dot detected in the long filename
		fit++;
	}
//	XPRINTF("fit3=%d\n", fit);
//	XPRINTF("Long name=%s  Short name=%s \n", lname, sname);

	//all 3 checks passed  - the long name fits in the short name without restrictions
	if (fit == 3) {
		XPRINTF("Short name is loseles!\n");
		return 0;
	}

	//one of the check failed - the short name have to be 'sequenced'
	//do not allow spaces in the short name
	for (i = 0; i < 8;i++) {
		if (sname[i] == 32) sname[i] = '_';
	}
	return 1;
}

/*
  separate path and filename
  fname - the source (merged) string (input)
  path  - separated path (output)
  name  - separated filename (output)
*/
int separatePathAndName(const char* fname, unsigned char* path, unsigned char* name) {
	int path_len;
	unsigned char *sp, *np;

	if(!(sp=strrchr(fname, '/')))	 //if last path separator missing ?
		np = (char *)fname;                  //  name starts at start of fname string
	else                           //else last path separator found
		np = sp+1;                   //  name starts after separator
	if(strlen(np) >= FAT_MAX_NAME) //if name is too long 
		return -ENAMETOOLONG;        //  return error code
	strcpy(name, np);              //copy name from correct part of fname string
	if((path_len = ((void *)np - (void *)fname)) >= FAT_MAX_PATH) //if path is too long
		return -ENAMETOOLONG;        //  return error code
	strncpy(path, fname, path_len);//copy path from start of fname string
	path[path_len] = 0;            //terminate path
	return 1;
}

/*
 get the sequence number from existing direntry name
*/
int getShortNameSequence(unsigned char* name, unsigned char* ext, unsigned char* sname) {
	int i,j;
	unsigned char* tmp;
	unsigned char buf[8];

	//at first: compare extensions
	//if extensions differ then filenames are diffrerent and seq is 0
	tmp = sname+8;
	for (i = 0; i < 3; i++) {
		if (ext[i] != tmp[i]) return 0;
	}

	//at second: find tilde '~' character (search backward from the end)
	for (i = 7; i > 0 && name[i] !='~'; i--);

	if (i == 0) return 0;  // tilde char was not found or is at first character

	//now compare the names - up to '~' position
	//if names differ then filenames are different;
	for (j=0; j < i;j++) {
		if (name[j] != sname[j]) return 0;
	}

	//if we get to this point we know that extension and name match
	//now get the sequence number behind the '~'
	for (j = i+1; j<8; j++) buf[j-i-1] = name[j];
	buf[j-i-1] = 0; //terminate

	XPRINTF("found short name sequence number='%s' \n", buf);
	return strtol((const char*)buf, NULL, 10);
}

/*
  set the short name sequence number
*/
int setShortNameSequence(unsigned char* entry, int maxEntry, unsigned char* sname) {
	char number[8];
	unsigned char *buf;
	int i,j;
	int maxSeq;
	int hit;
	int seq;
	int cont;

	seq = 0;
	maxSeq = 0;
	hit = 0;
	//search the entries for sequence number
	for (i = 0; i < maxEntry; i++) {
		seq = (entry[i] & 0xFC) >> 2;
		if (seq && i && (entry[i-1] & 0x03) == 2) { // seq != 0 && i != 0 && prceeding entry is long name
			seq += (entry[i-1] & 0xFC) << 4;
		}

		if (seq > maxSeq) maxSeq = seq; //find max sequence number
		if (seq) hit++; //find any sequence number
	}

	//check how many sequence numbers we have found and the highest sequence number.
	//if we get: hit==3 and maxSeq==120 then try to find unassigned sequence number
	if (hit < maxSeq) {
		cont = 1;
		for (j = 0; j < maxSeq && cont; j++) {
			//search the entries for sequence number
			for (i = 0; i < maxEntry && cont; i++) {
				seq = (entry[i] & 0xFC) >> 2;
				if (seq && i && (entry[i-1] & 0x03) == 2) { // seq != 0 && i != 0 && prceeding entry is long name
					seq += (entry[i-1] & 0xFC) << 4;
				}
				if (seq==j) cont = 0; //we found the sequence nuber - do not continue
			}
			if (cont == 1) { //the sequence number was missing in the entries
				seq = j;
			}
			cont = 1-cont; //set continue flag according the search result;
		}
	} else {
		seq = maxSeq + 1;
	}

	memset(number, 0, 8);
	sprintf(number, "%d",seq);
	j = strlen(number);

	buf = sname + 7  - j;
	buf[0] = '~';
	buf++;
	for (i = 0; i < j; i++) buf[i] = number[i];

	return 0;
}

/*
  find space where to put the direntry
*/
int getDirentryStoreOffset(unsigned char* entry, int entryCount, unsigned char* lname, int* direntrySize ) {
	int i;
	int size;
	int tightIndex;
	int looseIndex;
	int cont;
	int id;
	int slotStart;
	int slotSize;


	//direntry size for long name + 1 additional direntry for short name
	size = getDirentrySize(lname) + 1;
	XPRINTF("Direntry size=%d\n", size);
	*direntrySize = size;

	//we search for sequence of deleted or empty entries (entry id = 0)
	//1) search the tight slot (that fits completely. ex: size = 3, and slot space is 3 )
	//2) search the suitable (loose) slot (ex: size = 3, slot space is 5)

	slotStart = -1;
	slotSize = 0;
	tightIndex = -1;
	looseIndex = -1;
	cont = 1;
	//search the entries for entry types
	for (i = 0; i < entryCount && cont; i++) {
		id = (entry[i] & 0x03);
		if (id == 0) { //empty entry
			if (slotStart >= 0) {
				slotSize++;
			} else {
				slotStart = i;
				slotSize = 1;
				XPRINTF("*Start slot at index=%d ",slotStart);
			}
		} else { //occupied entry
			if (tightIndex < 0 && slotSize == size) {
				tightIndex = slotStart;
				XPRINTF("!Set tight index= %d\n", tightIndex);
			}
			if (looseIndex < 0 && slotSize > size) {
				looseIndex = slotStart + slotSize - size;
				XPRINTF("!Set loose index= %d\n", looseIndex);
			}
			if (tightIndex >= 0 && looseIndex >= 0) {
				cont = 0;
			}
			slotStart = -1;
			slotSize = 0;
		}
	}
	XPRINTF("\n");

	// tight index - smaller fragmentation of space, the larger blocks
	//               are left for larger filenames.
	// loose index - more fragmentation of direntry space, but the direntry
	//               name has space for future enlargement (of the name).
	//               i.e. no need to reposition the direntry and / or
	//               to allocate additional fat cluster for direntry space.

	// we prefere tight slot if available, othervise we use the loose slot.
	// if there are no tight and no loose slot then we position new direntry
	// at the end of current direntry space
	if (tightIndex >=0) {
		return tightIndex;
	}
	if (looseIndex >=0) {
		return looseIndex;
	}
	//last entry is (most likely) empty - use it
	if ( (entry[entryCount-1]&0x03) == 0) {
		return entryCount - 1;
	}

	return entryCount;

}



/*
  scans current directory entries and fills the info records

  lname        - long filename (to test wether existing entry match the long name) (input)
  sname        - short filename ( dtto ^^ ) (input)
  startCluster - valid start cluster of the directory or 0 if we scan the root directory (input)
  record       - info buffer (output)
  maxRecord    - size of the info buffer to avoid overflow

  if file/directory already exist (return code 0) then:
  retSector    - contains sector number of the direntry (output)
  retOffset    - contains byte offse of the SFN direntry from start of the sector (output)
  the reason is to speed up modification of the SFN (size of the file)

*/
int fat_fillDirentryInfo(fat_bpb* bpb, unsigned char* lname, unsigned char* sname, char directory, unsigned int* startCluster, unsigned char* record, int maxRecord, unsigned int* retSector, int* retOffset) {
	fat_direntry_sfn* dsfn;
	fat_direntry_lfn* dlfn;
	fat_direntry dir;
	int i, j;
	int dirSector;
	unsigned int startSector;
	unsigned int theSector;
	int cont;
	int ret;
	int dirPos;
	int clusterMod;
	int seq;

	cont = 1;
	clusterMod = bpb->clusterSize - 1;
	//clear name strings
	dir.sname[0] = 0;
	dir.name[0] = 0;

	j = 0;
	if (directory) directory = 0x10;

	fat_getDirentrySectorData(bpb, startCluster, &startSector, &dirSector);

	XPRINTF("dirCluster=%i startSector=%i (%i) dirSector=%i \n", *startCluster, startSector, startSector * Size_Sector, dirSector);

	//go through first directory sector till the max number of directory sectors
	//or stop when no more direntries detected
	for (i = 0; i < dirSector && cont; i++) {
		theSector = startSector + i;
		ret = READ_SECTOR(theSector, sbuf);
		if (ret < 0) {
			printf("FAT driver: read directory sector failed ! sector=%i\n", theSector);
			return -EIO;
		}
		XPRINTF("read sector ok, scanning sector for direntries...\n");

		//get correct sector from cluster chain buffer
		if ((*startCluster != 0) && (i % bpb->clusterSize == clusterMod)) {
			startSector = fat_cluster2sector(bpb, cbuf[(i / bpb->clusterSize) +  1]);
			startSector -= (i+1);
		}
		dirPos = 0;

		// go through start of the sector till the end of sector
		while (cont &&  dirPos < bpb->sectorSize && j< maxRecord) {
			dsfn = (fat_direntry_sfn*) (sbuf + dirPos);
			dlfn = (fat_direntry_lfn*) (sbuf + dirPos);
			cont = fat_getDirentry(dsfn, dlfn, &dir); //get single directory entry from sector buffer
			switch (cont) {
				case 1: //short name
					if (!(dir.attr & 0x08)) { //not volume label
						//file we want to create already exist - return the cluster of the file
						if ((strEqual(dir.sname, lname) == 0) || (strEqual(dir.name, lname) == 0) ) {
							//found directory but requested is file (and vice veresa)
							if ((dir.attr & 0x10) != directory) {
								if (directory) return -ENOTDIR;
								return -EISDIR;
							}
							XPRINTF("I: entry found! %s, %s = %s\n", dir.name, dir.sname, lname);
							*retSector = theSector;
							*retOffset = dirPos;
							*startCluster = dir.cluster;
							deSec[deIdx] = theSector;
							deOfs[deIdx] = dirPos;
							deIdx++;
							return 0;
						}
						record[j] = 1;
						seq = getShortNameSequence(dsfn->name, dsfn->ext, sname);
						record[j] += ((seq & 0x3F) << 2);
						if (seq > 63) {   //we need to store seq number in the preceding record
							if (j>0 && (record[j-1] & 0x03) == 2) {
								record[j-1] += ((seq & 0x0FC0) >> 4);
							} else {
								return -EFAULT;
							}
						}
						deIdx = 0;
						//clear name strings
						dir.sname[0] = 0;
						dir.name[0] = 0;
					} else {
						record[j] = 3;
						deIdx = 0;
					}
					break;
				case 2: record[j] = 2; //long name
					deSec[deIdx] = theSector;
					deOfs[deIdx] = dirPos;
					deIdx++;
					break;
				case 3: record[j] = 0; //empty
					deIdx = 0;
					break;
			}
			dirPos += 32; //directory entry of size 32 bytes
			j++;
		}
	}
	//indicate inconsistency
	if (j>= maxRecord) {
		j++;
	}
	return j;
}

/*
  check wether the new direntries (note: one file have at least 2 direntries for 1 SFN and 1..n LFN)
  fit into the current directory space.
  Enlarges the directory space if needed and possible (note: root dirspace can't be enlarged for fat12 and fat16)

  startCluster - valid start cluster of dirpace or 0 for the root directory
  entryCount   - number of direntries of the filename (at least 2)
  entryIndex   - index where to store new direntries
  direntrySize - number of all direntries in the directory
*/

int enlargeDirentryClusterSpace(fat_bpb* bpb, unsigned int startCluster, int entryCount, int entryIndex, int direntrySize)
{
	int ret;
	int dirSector;
	unsigned int startSector;
	int i;
	int maxSector;
	int entriesPerSector;
	int chainSize;
	unsigned int currentCluster;
	unsigned int newCluster;

	i = entryIndex + direntrySize;
	XPRINTF("cur=%d ecount=%d \n", i, entryCount);
	//we don't need to enlarge directory cluster space
	if (i <= entryCount) return 0; //direntry fits into current space

	entriesPerSector = bpb->sectorSize / 32;
	maxSector = i / entriesPerSector;
	if (i%entriesPerSector) {
		maxSector++;
	}

	chainSize = fat_getDirentrySectorData(bpb, &startCluster, &startSector, &dirSector);

	XPRINTF("maxSector=%d  dirSector=%d\n", maxSector, dirSector);

	if (maxSector<=dirSector) return 0;

	//Root directory of FAT12 or FAT16 - space can't be enlarged!
	if (startCluster == 0 && bpb->fatType < FAT32) {
		return -EMLINK; //too many direntries in the root directory
	}

	//in the cbuf we have the cluster chain

	//get last cluster of the cluster chain
	currentCluster = cbuf[chainSize-1];
	XPRINTF("current (last) cluster=%d \n", currentCluster);

	//get 1 cluster from cluster stack and append the chain
	newCluster = fat_getFreeCluster(bpb, currentCluster);
	XPRINTF("new cluster=%d \n", newCluster);
	fat_invalidateLastChainResult(); //prevent to misuse current (now updated) cbuf
	//if new cluster cannot be allocated
	if (newCluster == 0) {
		return -ENOSPC;
	}

	// now clean the directory space
	startSector = fat_cluster2sector(bpb, newCluster);
	for (i = 0; i < bpb->clusterSize; i++) {
		ret = ALLOC_SECTOR(startSector + i, sbuf);
		memset(sbuf, 0 , bpb->sectorSize); //fill whole sector with zeros
		ret = WRITE_SECTOR(startSector + i);
		if (ret < 0) return -EIO;
	}
	return 1; // 1 cluster allocated
}


/*
  Create direntries of the long and short filename to the supplied buffer.

  lname     : long name
  sname     : short name
  directory : 0-file, 1-directory
  buffer    : start of the data buffer where to store direntries
  cluster   : start cluster of the file/directory

  returns the number of the direntries (3 means: 2 entries for long name + 1 entry for short name)
  note: the filesize set in the direntry is 0 (for both directory and file)
*/
int createDirentry(unsigned char* lname, unsigned char* sname, char directory, unsigned int cluster, unsigned char* buffer) {
	int i;
	int lsize;
	int nameSize;
	unsigned char chsum;

	lsize = getDirentrySize(lname) - 1;
	chsum = computeNameChecksum(sname);
	nameSize = strlen((const char*) lname);
	//if the long name is longer than 8.3 chars then more than 1 lfn direntry have to be made
	for (i = 0; i <= lsize; i++) {
		setLfnEntry(lname, nameSize, chsum, buffer, lsize-i, lsize);
	}
	lsize++;
	//now create direntry of the short name right behind the long name direntries
	setSfnEntry(sname, directory, buffer + (lsize * 32), cluster);
	return lsize + 1;
}

/*
  Create empty directory space with two SFN direntries:
  1) current directory "."
  2) parent directory  ".."

*/
int createDirectorySpace(fat_bpb* bpb, unsigned int dirCluster, unsigned int parentDirCluster) {
	int i;
	int ret;
	unsigned int startSector;
	unsigned char name[11];

	//we do not mess with root directory
	if (dirCluster < 2) {
		return -EFAULT;
	}

	for (i = 0; i< 11; i++) name[i] = 32;
	name[0] = '.';
	setSfnEntry(name, 1, tbuf, dirCluster);
	name[1] = '.';
	setSfnEntry(name, 1, tbuf + 32, parentDirCluster);

	//we create directory space inside one cluster. No need to worry about
	//large dir space spread on multiple clusters
	startSector = fat_cluster2sector(bpb, dirCluster);
	XPRINTF("I: create dir space: cluster=%d sector=%d (%d) \n", dirCluster, startSector, startSector * bpb->sectorSize);

	//go through all sectors of the cluster
	for (i = 0; i < bpb->clusterSize; i++) {
		ret = READ_SECTOR(startSector + i, sbuf);
		if (ret < 0) {
			printf("FAT writer: read directory sector failed ! sector=%i\n", startSector + i);
			return -EIO;
		}
		memset(sbuf, 0, bpb->sectorSize); //clean the sector
		if (i == 0) {
			memcpy(sbuf, tbuf, 64);
		}
		ret = WRITE_SECTOR(startSector + i);
		if (ret < 0) {
			printf("FAT writer: write directory sector failed ! sector=%i\n", startSector + i);
			return -EIO;
		}
	}

    return(0);
}


/*
  save direntries stored in dbuf to the directory space on the disk

  startCluster - start cluster of the directory space
  dbuf         - direntry buffer
  entrySize    - number of direntries stored in the dbuf
  entryIndex   - index of the direntry start in the directory space

  retSector    - contains sector number of the direntry (output)
  retOffset    - contains byte offse of the SFN direntry from start of the sector (output)
  the reason is to speed up modification of the SFN (size of the file)

*/
int saveDirentry(fat_bpb* bpb, unsigned int startCluster, unsigned char * dbuf, int entrySize, int entryIndex, unsigned int* retSector, int* retOffset) {
	int i, j;
	int dirSector;
	unsigned int startSector;
	unsigned int theSector;
	int cont;
	int ret;
	int dirPos;
	int clusterMod;
	int entryEndIndex;
	int writeFlag;

	cont = 1;
	clusterMod = bpb->clusterSize - 1;
	//clear name strings
	entryEndIndex = entryIndex + entrySize;

	j = 0;

	fat_getDirentrySectorData(bpb, &startCluster, &startSector, &dirSector);

	XPRINTF("dirCluster=%i startSector=%i (%i) dirSector=%i \n", startCluster, startSector, startSector * Size_Sector, dirSector);

	//go through first directory sector till the max number of directory sectors
	//or stop when no more direntries detected
	for (i = 0; i < dirSector && cont; i++) {
		theSector = startSector + i;
		ret = READ_SECTOR(theSector, sbuf);
		if (ret < 0) {
			printf("FAT writer: read directory sector failed ! sector=%i\n", theSector);
			return -EIO;
		}
		XPRINTF("read sector ok, scanning sector for direntries...\n");

		//prepare next sector: get correct sector from cluster chain buffer
		if ((startCluster != 0) && (i % bpb->clusterSize == clusterMod)) {
			startSector = fat_cluster2sector(bpb, cbuf[(i / bpb->clusterSize) +  1]);
			startSector -= (i+1);
		}
		dirPos = 0;
		writeFlag = 0;
		// go through start of the sector till the end of sector
		while (dirPos < bpb->sectorSize) {
			if (j >=entryIndex && j < entryEndIndex) {
				memcpy(sbuf + dirPos, dbuf + ((j-entryIndex)*32), 32);
				writeFlag++;
				//SFN is stored
				if (j == entryEndIndex-1) {
					*retSector = theSector;
					*retOffset = dirPos;
				}
			}
			//sbuf + dirPos
			dirPos += 32; //directory entry of size 32 bytes
			j++;
		}
		//store modified sector
		if (writeFlag) {
			ret = WRITE_SECTOR(theSector);
			if (ret < 0) {
				printf("FAT writer: write directory sector failed ! sector=%i\n", theSector);
				return -EIO;
			}
		}
		if (j >= entryEndIndex) {
			cont = 0; //do not continue
		}
	}
	return j;
}


/*
  - create/convert long name to short name
  - analyse directory space
  - enlarge directory space
  - save direntries

  lname         - long name
  directory     - 0-> create file  1->create directory
  escapeNoExist - 0->allways create file  1->early exit if file doesn't exist
  startCluster  - directory space start cluster (set to zero for root directory)
  retSector     - SFN sector - sector of the SFN direntry (output)
  retOffset     - byte offset of the SFN direntry counting from the start of the sector (output)
*/

int fat_modifyDirSpace(fat_bpb* bpb, unsigned char* lname, char directory, char escapeNotExist, unsigned int* startCluster, unsigned int* retSector, int* retOffset) {
	int ret;
	unsigned char sname[12]; //short name 8+3 + terminator
	unsigned int newCluster;
	int entryCount;
	int compressShortName;
	int entryIndex;
	int direntrySize;

	//memo buffer for each direntry - up to 1024 entries in directory
	// 7 6 5 4 3 2 | 1 0
        // ------------+-----
        // SEQ HI/LO   | ID
 	// ------------------

	// ID : 0 - entry is empty or deleted
	//      1 - sfn entry
	//      2 - lfn entry
	//      3 - other entry (volume label etc.)
	// SEQ: sequence number of the short filename.
	// if our long filename is "Quitelongname.txt" then
	// seq for existing entry:
	// ABCD.TXT     seq = 0 (no sequence number in name and name doesn't match)
	// ABCDEF~1.TXT seq = 0 (because the short names doesn't match)
	// QUITELON.TXT seq = 0 (again the short name doesn't match)
	// QUITEL~1.JPG seq = 0 (name match but extension desn't match)
	// QUITEL~1.TXT seq = 1 (both name and extension match - sequence number 1)
	// QUITE~85.TXT seq = 85 ( dtto. ^^^^)

	// If the sfn has sequence it means the filename should be long
	// and preceeding entry should be lfn. In this case the preceeding (lfn)
	// entry seq holds the high 6 bites of the whole sequence. If preceding
	// entry is another sfn direntry then we report error (even if it might be
	// (in some rare occasions) correct directory structure).


	unsigned char entry[DIRENTRY_COUNT];
	memset(entry, 0, DIRENTRY_COUNT);
	sname[11] = 0;

	//create short name from long name
	ret = createShortNameMask(lname,sname);
	if (ret < 0) {
		XPRINTF("E: short name invalid!\n");
		return ret;
	}
	compressShortName = ret;

	//get information about existing direntries (palcement of the empty/reusable direntries)
	//and sequence nubers of the short filenames
	ret = fat_fillDirentryInfo(bpb, lname, sname, directory, startCluster, entry, DIRENTRY_COUNT, retSector, retOffset);
	if (ret < 0) {
		XPRINTF("E: direntry data invalid!\n");
		return ret;
	}
	//ret 0 means that exact filename/directory already exist
	if (ret == 0) {
		return ret;
	}

	//exact filename not exist and we want to report it
	if (escapeNotExist) {
		return -ENOENT;
	}


	if (ret > DIRENTRY_COUNT) {
		XPRINTF("W: Direntry count is larger than number of records!\n");
		ret = DIRENTRY_COUNT;
	}
	entryCount = ret;
	XPRINTF("I: direntry count=%d\n", entryCount);

	if (compressShortName) {
		setShortNameSequence(entry, entryCount, sname);
	}
	XPRINTF("I: new short name='%s' \n", sname);


	//find the offset (index) of the direntry space where to put this direntry
	entryIndex = getDirentryStoreOffset(entry, entryCount, lname, &direntrySize);
	XPRINTF("I: direntry store offset=%d\n", entryIndex);

	//if the direntry offset excede current space of directory clusters
	//we have to add one cluster to directory space
	ret = enlargeDirentryClusterSpace(bpb, *startCluster, entryCount, entryIndex, direntrySize);
	XPRINTF("I: enlarge direntry cluster space ret=%d\n", ret);
	if (ret < 0) {
		return ret;
	}

	//get new cluster for file/directory
	newCluster = fat_getFreeCluster(bpb, 0);
	if (newCluster == 0) {
		return -EFAULT;
	}
	XPRINTF("I: new file/dir cluster=%d\n", newCluster);

	//create direntry data to temporary buffer
	ret = createDirentry(lname, sname, directory, newCluster, tbuf);
	XPRINTF("I: create direntry to buf ret=%d\n", ret);
	if (ret < 0) {
		return ret;
	}
	direntrySize = ret;

	//now store direntries into the directory space
	ret = saveDirentry(bpb, *startCluster, tbuf, direntrySize, entryIndex, retSector, retOffset);
	XPRINTF("I: save direntry ret=%d\n", ret);
	if (ret < 0) {
		return ret;
	}

	//create empty directory structure
	if (directory) {
		ret = createDirectorySpace(bpb, newCluster, *startCluster);
		XPRINTF("I: create directory space ret=%d\n", ret);
		if (ret < 0) {
			return ret;
		}
	}

	return 1;

}


/*
   Check wether directory space contain any file or directory

   startCluster - start cluster of the directory space

   returns: 0 - false - directory space contains files or directories (except '.' and '..')
            1 - true  - directory space is empty or contains deleted entries
           -X - error
*/
int checkDirspaceEmpty(fat_bpb* bpb, unsigned int startCluster, unsigned char* entry) {
	int ret;
	int i;
	unsigned char sname[12]; //short name 8+3 + terminator
	int entryCount;

	unsigned int retSector;
	int retOffset;

	XPRINTF("I: checkDirspaceEmpty  directory cluster=%d \n", startCluster);
	if (startCluster < 2) {  // do not check root directory!
		return -EFAULT;
	}

	memset(entry, 0, DIRENTRY_COUNT);
	sname[0] = 0;


	ret = fat_fillDirentryInfo(bpb, sname, sname, 1, &startCluster, entry, DIRENTRY_COUNT, &retSector, &retOffset);
	if (ret > DIRENTRY_COUNT) {
		XPRINTF("W: Direntry count is larger than number of records! directory space cluster =%d maxRecords=%d\n", startCluster, DIRENTRY_COUNT);
		ret = DIRENTRY_COUNT;
	}
	entryCount = ret;

	for (i= 2; i < entryCount; i++) {  //first two records should be '.' & '..'
		if ((entry[i] & 0x03) != 0) {
			XPRINTF("I: directory not empty!\n");
			return 0;
		}
	}
	XPRINTF("I: directory is empty.\n");
	return 1;
}

/*
   Remove the name (direntries of the file or directory) from the directory space.

   lname        - long name (without the path)
   directory    - 0->delete file    1-delete directory
   startCluster - start cluster of the directory space
*/

int fat_clearDirSpace(fat_bpb* bpb, unsigned char* lname, char directory, unsigned int* startCluster) {
	int ret;
	int i;
	unsigned char sname[12]; //short name 8+3 + terminator
	unsigned int dirCluster;
	unsigned int theSector;
	unsigned int sfnSector;
	int sfnOffset;
	unsigned char entry[DIRENTRY_COUNT];

	memset(entry, 0, DIRENTRY_COUNT);
	sname[0] = 0;


	dirCluster = *startCluster;
	//get information about existing direntries (palcement of the empty/reusable direntries)
	//and find the lname in the directory
	//also fill up the dsSec and dsOfs information
	ret = fat_fillDirentryInfo(bpb, lname, sname, directory, startCluster, entry, DIRENTRY_COUNT, &sfnSector, &sfnOffset);
	if (ret != 0) {
		XPRINTF("E: direntry not found!\n");
		return -ENOENT;
	}
	XPRINTF("clear dir space: dir found at  cluster=%d \n ", *startCluster);

	//Check wether any file or directory exist in te target directory space.
	//We should not delete the directory if files/directories exist
	//because just clearing the dir doesn't free the fat clusters
	//occupied by files.
	if (directory) {
		//check wether sub-directory is empty
		//startCluster now points to subdirectory start cluster
		ret = checkDirspaceEmpty(bpb, *startCluster, entry);
		if (ret == 0) { //directorty contains some files
			return -ENOTEMPTY;
		}
		//read the direntry info again, because we lost it during subdir check
		*startCluster = dirCluster;
		memset(entry, 0, DIRENTRY_COUNT);

		ret = fat_fillDirentryInfo(bpb, lname, sname, directory, startCluster, entry, DIRENTRY_COUNT, &sfnSector, &sfnOffset);
		if (ret != 0) {
			XPRINTF("E: direntry not found!\n");
			return -ENOENT;
		}
	}

	//now mark direntries as deleted
	theSector = 0;
	for (i = 0; i < deIdx; i++) {
	        if (deSec[i] != theSector) {
			if (theSector > 0) {
				ret = WRITE_SECTOR(theSector);
				if (ret < 0) {
					printf("FAT writer: write directory sector failed ! sector=%i\n", theSector);
					return -EIO;
				}
			}
			theSector = deSec[i];
			ret = READ_SECTOR(theSector, sbuf);
			if (ret < 0) {
				printf("FAT writer: read directory sector failed ! sector=%i\n", theSector);
				return -EIO;
			}
		}
		sbuf[deOfs[i]] = 0xE5; //delete marker
	}
	if (theSector > 0) {
		ret = WRITE_SECTOR(theSector);
		if (ret < 0) {
			printf("FAT writer: write directory sector failed ! sector=%i\n", theSector);
			return -EIO;
		}
	}

	//now delete whole cluster chain starting at the file's first cluster
	ret = fat_deleteClusterChain(bpb, *startCluster);
	if (ret < 0) {
		XPRINTF("E: delete cluster chain failed!\n");
		return ret;
	}
	FLUSH_SECTORS();
	return 1;
}




/* ===================================================================== */

/*
  cluster  - start cluster of the file
  sfnSector - short filename entry sector
  sfnOffset - short filename entry offset from the sector start
*/
int fat_truncateFile(fat_bpb* bpb, unsigned int cluster, unsigned int sfnSector, int sfnOffset ) {
	int ret;
	fat_direntry_sfn* dsfn;

	if (cluster == 0 || sfnSector == 0) {
		return -EFAULT;
	}


	//now delete whole cluster chain starting at the file's first cluster
	ret = fat_deleteClusterChain(bpb, cluster);
	if (ret < 0) {
		XPRINTF("E: delete cluster chain failed!\n");
		return ret;
	}

	//terminate cluster
	ret = fat_createClusterChain(bpb, cluster);
	if (ret < 0) {
		XPRINTF("E: truncate cluster chain failed!\n");
		return ret;
	}

	ret = READ_SECTOR(sfnSector, sbuf);
	if (ret < 0) {
		printf("FAT writer: read direntry sector failed ! sector=%i\n", sfnSector);
		return -EIO;
	}
	dsfn = (fat_direntry_sfn*) (sbuf + sfnOffset);
	dsfn->size[0] = 0;
	dsfn->size[1] = 0;
	dsfn->size[2] = 0;
	dsfn->size[3] = 0;

	ret = WRITE_SECTOR(sfnSector);
	if (ret < 0) {
		printf("FAT writer: write directory sector failed ! sector=%i\n", sfnSector);
		return -EIO;
	}
	return 1;
}


/*
  Update size of the SFN entry

  cluster  - start cluster of the file
  sfnSector - short filename entry sector
  sfnOffset - short filename entry offset from the sector start
*/
int fat_updateSfn(int size, unsigned int sfnSector, int sfnOffset ) {
	int ret;
	fat_direntry_sfn* dsfn;

	if (sfnSector == 0) {
		return -EFAULT;
	}


	ret = READ_SECTOR(sfnSector, sbuf);
	if (ret < 0) {
		printf("FAT writer: read direntry sector failed ! sector=%i\n", sfnSector);
		return -EIO;
	}
	dsfn = (fat_direntry_sfn*) (sbuf + sfnOffset);
	dsfn->size[0] = size & 0xFF;
	dsfn->size[1] = (size & 0xFF00) >> 8;
	dsfn->size[2] = (size & 0xFF0000) >> 16;
	dsfn->size[3] = (size & 0xFF000000) >> 24;

	setSfnDate(dsfn, DATE_MODIFY);

	ret = WRITE_SECTOR(sfnSector);
	if (ret < 0) {
		printf("FAT writer: write directory sector failed ! sector=%i\n", sfnSector);
		return -EIO;
	}
	XPRINTF("I: sfn updated, file size=%d \n", size);
	return 1;
}


/*
 create file or directory

 fname          - path and filename
 directory      - set to 0 to create file, 1 to create directory
 escapeNotExist - set to 1 if you want to report error if file not exist.
                  Otherwise set to 0 and file/dir will be created.
 cluster        - start cluster of the directory space - default is 0 -> root directory
 sfnSector      - sector of the SFN entry (output) - helps to modify direntry (size, date, time)
 sfnOffset      - offset (in bytes) of the SFN entry (output)
*/

int fat_createFile(fat_bpb* bpb, const char* fname, char directory, char escapeNotExist, unsigned int* cluster, unsigned int* sfnSector, int* sfnOffset) {
	int ret;
	unsigned int startCluster;
	unsigned int directoryCluster;
	unsigned char path[FAT_MAX_PATH];
	unsigned char lname[FAT_MAX_NAME];


	ret = separatePathAndName(fname, path, lname);
	if (lname[0] == 0 || lname[0]=='.' || ret < 0) {
		XPRINTF("E: file name not exist or not valid!");
		return -ENOENT;
	}

	//get start cluster of the last sub-directory of the path
	startCluster = 0;
	ret = fat_getFileStartCluster(bpb, (const char*)path, &startCluster, NULL);
	if (ret > 0) {
		XPRINTF("directory=%s name=%s cluster=%d \n", path, lname, startCluster);
	}else {
		XPRINTF("E: directory not found! \n");
		return -ENOENT;
	}

	//modify directory space of the path (cread direntries)
	//and/or create new (empty) directory space if directory creation requested
	directoryCluster = startCluster;
	ret = fat_modifyDirSpace(bpb, lname, directory, escapeNotExist, &startCluster, sfnSector, sfnOffset);
	if (ret < 0) {
		XPRINTF("E: modifyDirSpace failed!\n");
		return ret;
	}
	XPRINTF("I: SFN info: sector=%d (%d)  offset=%d (%d) startCluster=%d\n", *sfnSector, *sfnSector * bpb->sectorSize, *sfnOffset, *sfnOffset + (*sfnSector * bpb->sectorSize), startCluster);
	*cluster = startCluster;
	//dlanor: I've repatched the stuff below to improve functionality
	//The simple test below was bugged for the case of creating a folder in root
	//if (startCluster != directoryCluster) {
	//That test (on the line above) fails in root because created folders never
	//get a startCluster of 0. That is reserved for root only.
	//The test below replaces this with a more complex test that takes all of this
	//stuff into proper account, but behaves like the old test for other cases.
	//That's mainly because I can't tell how consistent name conflict flagging is
	//for those other cases, and it's not really worth the trouble of finding out :)
	if( (ret == 0) //if we have a directly flagged name conflict
		||( (directoryCluster || !directory)   //OR working in non_root, or making a file
			&&(startCluster != directoryCluster) //AND we get an unexpected startCluster
			)
		) {
		XPRINTF("I: file already exists at cluster=%d\n", startCluster);
		return 2;
	}
	return 1;
}


int fat_deleteFile(fat_bpb* bpb, const char* fname, char directory) {
	int ret;
	unsigned int startCluster;
	unsigned int directoryCluster;
	unsigned char path[FAT_MAX_PATH];
	unsigned char lname[FAT_MAX_NAME];


	ret = separatePathAndName(fname, path, lname);
	if (lname[0] == 0 || lname[0]=='.' || ret < 0) {
		XPRINTF("E: file name not exist or not valid!");
		return -ENOENT;
	}

	//get start cluster of the last sub-directory of the path
	startCluster = 0;
	ret = fat_getFileStartCluster(bpb, (const char*)path, &startCluster, NULL);
	if (ret > 0) {
		XPRINTF("directory=%s name=%s cluster=%d \n", path, lname, startCluster);
	}else {
		XPRINTF("E: directory not found! \n");
		return -ENOENT;
	}

	//delete direntries and modify fat
	directoryCluster = startCluster;
	ret = fat_clearDirSpace(bpb, lname, directory, &startCluster);
	if (ret < 0) {
		XPRINTF("E: cleanDirSpace failed!\n");
		return ret;
	}
	if (startCluster != directoryCluster) {
		XPRINTF("I: file/dir removed from cluster=%d\n", startCluster);
	}
	return 1;
}


int fat_writeFile(fat_bpb* bpb, fat_dir* fatDir, int* updateClusterIndices, unsigned int filePos, unsigned char* buffer, int size) {
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
	unsigned int endPosFile;
	unsigned int endPosCluster;
	unsigned int lastCluster;

	int clusterChainStart;


	//check wether we have enough clusters allocated
	i = bpb->clusterSize * bpb->sectorSize; //the size (in bytes) of the one cluster
	j = fatDir->size / i;
	if (fatDir->size % i) {
		j++;
	}
	if (j == 0) j = 1; //the file have allways at least one cluster allocated

	endPosCluster = j * i;
	endPosFile = filePos + size;

	*updateClusterIndices = 0;

        //allocate additional cluster(s)
	if (endPosFile > endPosCluster) {
		ret = endPosFile - endPosCluster; //additional space needed in bytes
		j = ret / i;  //additional space needed (given in number of clusters)
		if (ret % i) {
			j++;
		}
		lastCluster = fatDir->lastCluster;
		XPRINTF("I: writeFile: last cluster= %d \n", lastCluster);

		if (lastCluster == 0) return -ENOSPC; // no more free clusters or data invalid
		for (i = 0; i < j; i++) {
			lastCluster = fat_getFreeCluster(bpb, lastCluster);
			if (lastCluster == 0) return -ENOSPC; // no more free clusters
		}
		fatDir->lastCluster = lastCluster;
		*updateClusterIndices = j;
		fat_invalidateLastChainResult(); //prevent to misuse current (now deleted) cbuf

		XPRINTF("I: writeFile: new clusters allocated = %d new lastCluster=%d \n", j, lastCluster);
	}
	XPRINTF("I: write file: filePos=%d  dataSize=%d \n", filePos, size);


	fat_getClusterAtFilePos(bpb, fatDir, filePos, &fileCluster, &clusterPos);
	sectorSkip = (filePos - clusterPos) / bpb->sectorSize;
	clusterSkip = sectorSkip / bpb->clusterSize;
	sectorSkip %= bpb->clusterSize;
	dataSkip  = filePos  % bpb->sectorSize;
	bufferPos = 0;


	XPRINTF("fileCluster = %i,  clusterPos= %i clusterSkip=%i, sectorSkip=%i dataSkip=%i \n",
		fileCluster, clusterPos, clusterSkip, sectorSkip, dataSkip);

	if (fileCluster < 2) {
		return -EFAULT;
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
				//TODO - do not read when writing to unallocated sectors
				ret = READ_SECTOR(startSector + j, sbuf);
				if (ret < 0) {
					printf("Read sector failed ! sector=%i\n", startSector + j);
					return bufferPos; //return number of bytes already written
				}

				//compute exact size of transfered bytes
				if (size < bufSize) {
					bufSize = size + dataSkip;
				}
				if (bufSize > SECTOR_SIZE) {
					bufSize = SECTOR_SIZE;
				}
				XPRINTF("memcopy dst=%i, src=%i, size=%i  bufSize=%i \n", dataSkip, bufferPos,bufSize-dataSkip, bufSize);
				MEMCPY(sbuf + dataSkip, buffer+bufferPos, bufSize - dataSkip);
				ret = WRITE_SECTOR(startSector + j);
				if (ret < 0) {
					printf("Write sector failed ! sector=%i\n", startSector + j);
					return bufferPos; //return number of bytes already written
				}

				size-= (bufSize - dataSkip);
				bufferPos +=  (bufSize - dataSkip);
				dataSkip = 0;
				bufSize = SECTOR_SIZE;
			}
			sectorSkip = 0;
		}
		clusterSkip = 0;
	}

	return bufferPos; //return number of bytes already written
}


int fat_allocSector(unsigned int sector, unsigned char** buf) {
	int ret;

	ret = ALLOC_SECTOR(sector, sbuf);
	if (ret < 0) {
		printf("Alloc sector failed ! sector=%i\n", sector);
		return -1;
	}
	*buf = sbuf;
	return Size_Sector;
}

int fat_writeSector(unsigned int sector) {
	int ret;

	ret = WRITE_SECTOR(sector);
	if (ret < 0) {
		printf("Write sector failed ! sector=%i\n", sector);
		return -1;
	}
	return Size_Sector;
}

int fat_flushSectors(void)
{
	FLUSH_SECTORS();
	return(0);
}
