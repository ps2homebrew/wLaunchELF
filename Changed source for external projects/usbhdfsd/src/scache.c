/*
 * scache.c - USB Mass storage driver for PS2
 *
 * (C) 2004, Marek Olejnik (ole00@post.cz)
 * (C) 2004  Hermes (support for sector sizes from 512 to 4096 bytes)
 *
 * Sector cache
 *
 * See the file LICENSE included with this distribution for licensing terms.
 */

#include <tamtypes.h>
#include <stdio.h>
#include <sysmem.h>
#define malloc(a)	AllocSysMemory(0,(a), NULL)
#define free(a)		FreeSysMemory((a))

#include "mass_stor.h"

// Modified Hermes
//always read 4096 bytes from sector (the rest bytes is stored in the cache)
#define READ_SECTOR_4096(a, b)	mass_stor_readSector4096((a), (b))
#define WRITE_SECTOR_4096(a, b)	mass_stor_writeSector4096((a), (b))

#include "mass_debug.h"

//number of cache slots (1 slot = memory block of 4096 bytes)
#define CACHE_SIZE 32

//when the flushCounter reaches FLUSH_TRIGGER then flushSectors is called
#define FLUSH_TRIGGER 16

typedef struct _cache_record
{
	unsigned int sector;
	int tax;
	char writeDirty;
} cache_record;


int sectorSize;
int indexLimit;
unsigned char* sectorBuf = NULL;		//sector content - the cache buffer
cache_record rec[CACHE_SIZE];	//cache info record

//statistical infos
unsigned int cacheAccess;
unsigned int cacheHits;
unsigned int writeFlag;
unsigned int flushCounter;

unsigned int cacheDumpCounter = 0;

void initRecords()
{
	int i;

	for (i = 0; i < CACHE_SIZE; i++)
	{
		rec[i].sector = 0xFFFFFFF0;
		rec[i].tax = 0;
		rec[i].writeDirty = 0;
	}

	writeFlag = 0;
	flushCounter = 0;
}

/* search cache records for the sector number stored in cache
  returns cache record (slot) number
 */
int getSlot(unsigned int sector) {
	int i;

	for (i = 0; i < CACHE_SIZE; i++) {
		if (sector >= rec[i].sector && sector < (rec[i].sector + indexLimit)) {
			return i;
		}
	}
	return -1;
}


/* search cache records for the sector number stored in cache */
int getIndexRead(unsigned int sector) {
	int i;
	int index =-1;

	for (i = 0; i < CACHE_SIZE; i++) {
		if (sector >= rec[i].sector && sector < (rec[i].sector + indexLimit)) {
			if (rec[i].tax < 0) rec[i].tax = 0;
			rec[i].tax +=2;
			index = i;
		}
		rec[i].tax--;     //apply tax penalty
	}
	if (index < 0)
		return index;
	else
		return ((index * indexLimit) + (sector - rec[index].sector));
}

/* select the best record where to store new sector */
int getIndexWrite(unsigned int sector) {
	int i, ret;
	int minTax = 0x0FFFFFFF;
	int index = 0;

	for (i = 0; i < CACHE_SIZE; i++) {
		if (rec[i].tax < minTax) {
			index = i;
			minTax = rec[i].tax;
		}
	}

	//this sector is dirty - we need to flush it first
	if (rec[index].writeDirty) {
		XPRINTF("scache: getIndexWrite: sector is dirty : %d   index=%d \n", rec[index].sector, index);
		ret = WRITE_SECTOR_4096(rec[index].sector, sectorBuf + (index * 4096));
		rec[index].writeDirty = 0;
		//TODO - error handling
		if (ret < 0) {
			printf("scache: ERROR writing sector to disk! sector=%d\n", sector);
		}

	}
	rec[index].tax +=2;
	rec[index].sector = sector;

	return index * indexLimit;
}



/*
	flush dirty sectors
 */
int scache_flushSectors() {
	int i,ret;
	int counter = 0;

	flushCounter = 0;

	XPRINTF("scache: flushSectors writeFlag=%d\n", writeFlag);
	//no write operation occured since last flush
	if (writeFlag==0) {
		return 0;
	}

	for (i = 0; i < CACHE_SIZE; i++) {
		if (rec[i].writeDirty) {
			XPRINTF("scache: flushSectors dirty index=%d sector=%d \n", i, rec[i].sector);
			ret = WRITE_SECTOR_4096(rec[i].sector, sectorBuf + (i * 4096));
			rec[i].writeDirty = 0;
			//TODO - error handling
			if (ret < 0) {
				printf("scache: ERROR writing sector to disk! sector=%d\n", rec[i].sector);
				return ret;
			}
			counter ++;
		}
	}
	writeFlag = 0;
	return counter;
}

int scache_readSector(unsigned int sector, void** buf) {
	int index; //index is given in single sectors not octal sectors
	int ret;
	unsigned int allignedSector;

	XPRINTF("cache: readSector = %i \n", sector);
	cacheAccess ++;
	index = getIndexRead(sector);
	XPRINTF("cache: indexRead=%i \n", index);
	if (index >= 0) { //sector found in cache
		cacheHits ++;
		*buf = sectorBuf + (index * sectorSize);
		XPRINTF("cache: hit and done reading sector \n");

		return sectorSize;
	}

	//compute allignedSector - to prevent storage of duplicit sectors in slots
	allignedSector = (sector/indexLimit)*indexLimit;
	index = getIndexWrite(allignedSector);
	XPRINTF("cache: indexWrite=%i slot=%d  allignedSector=%i\n", index, index / indexLimit, allignedSector);
	ret = READ_SECTOR_4096(allignedSector, sectorBuf + (index * sectorSize));

	if (ret < 0) {
		return ret;
	}
	*buf = sectorBuf + (index * sectorSize) + ((sector%indexLimit) * sectorSize);
	XPRINTF("cache: done reading physical sector \n");

	//write precaution
	flushCounter++;
	if (flushCounter == FLUSH_TRIGGER) {
		scache_flushSectors();
	}

	return sectorSize;
}


int scache_allocSector(unsigned int sector, void** buf) {
	int index; //index is given in single sectors not octal sectors
	//int ret;
	unsigned int allignedSector;

	XPRINTF("cache: allocSector = %i \n", sector);
	index = getIndexRead(sector);
	XPRINTF("cache: indexRead=%i \n", index);
	if (index >= 0) { //sector found in cache
		*buf = sectorBuf + (index * sectorSize);
		XPRINTF("cache: hit and done allocating sector \n");
		return sectorSize;
	}

	//compute allignedSector - to prevent storage of duplicit sectors in slots
	allignedSector = (sector/indexLimit)*indexLimit;
	index = getIndexWrite(allignedSector);
	XPRINTF("cache: indexWrite=%i \n", index);
	*buf = sectorBuf + (index * sectorSize) + ((sector%indexLimit) * sectorSize);
	XPRINTF("cache: done allocating sector\n");
	return sectorSize;
}


int scache_writeSector(unsigned int sector) {
	int index; //index is given in single sectors not octal sectors
	//int ret;

	XPRINTF("cache: writeSector = %i \n", sector);
	index = getSlot(sector);
	if (index <  0) { //sector not found in cache
		XPRINTF("cache: writeSector: ERROR! the sector is not allocated! \n");
		return -1;
	}
	XPRINTF("cache: slotFound=%i \n", index);

	//prefere written sectors to stay in cache longer than read sectors
	rec[index].tax += 2;

	//set dirty status
	rec[index].writeDirty++;
	writeFlag++;

	XPRINTF("cache: done soft writing sector \n");


	//write precaution
	flushCounter++;
	if (flushCounter == FLUSH_TRIGGER) {
		scache_flushSectors();
	}

	return sectorSize;
}

int scache_init(char * param, int sectSize)
{
	//added by Hermes
	sectorSize = sectSize;
	indexLimit = 4096/sectorSize; //number of sectors per 1 cache slot

	if (sectorBuf == NULL) {
		XPRINTF("scache init! \n");
		XPRINTF("sectorSize: 0x%x\n",sectorSize);
		sectorBuf = (unsigned char*) malloc(4096 * CACHE_SIZE ); //allocate 4096 bytes per 1 cache record
		if (sectorBuf == NULL) {
			XPRINTF("Sector cache: can't alloate memory of size:%d \n", 4096 * CACHE_SIZE);
			return -1;
		}
		XPRINTF("Sector cache: alocated memory at:%p of size:%d \n", sectorBuf, 4096 * CACHE_SIZE);
	} else {
		XPRINTF("scache flush! \n");
	}
	cacheAccess = 0;
	cacheHits = 0;
	initRecords();
	return(1);
}

void scache_getStat(unsigned int* access, unsigned int* hits) {
	*access = cacheAccess;
        *hits = cacheHits;
}

void scache_close()
{
	scache_flushSectors();
	if(sectorBuf != NULL)
	{
		free(sectorBuf);
		sectorBuf = NULL;
	}
}
