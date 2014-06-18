#ifndef _SCACHE_H
#define _SCACHE_H 1

int  scache_init(char * param, int sectorSize);
void scache_close();
void scache_kill(); //dlanor: added for disconnection events (flush impossible)
int  scache_allocSector(unsigned int sector, void** buf);
int  scache_readSector(unsigned int sector, void** buf);
int  scache_writeSector(unsigned int sector);
int  scache_flushSectors();

void scache_getStat(unsigned int* access, unsigned int* hits);
void scache_dumpRecords();
#endif 
