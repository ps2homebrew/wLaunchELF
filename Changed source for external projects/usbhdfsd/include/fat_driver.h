#ifndef _FAT_DRIVER_H
#define _FAT_DRIVER_H 1

#include "fat.h"

int fat_mountCheck();
void fat_setFatDirChain(fat_bpb* bpb, fat_dir* fatDir);
int fat_readFile(fat_bpb* bpb, fat_dir* fatDir, unsigned int filePos, unsigned char* buffer, int size);
int fat_getFirstDirentry(char * dirName, fat_dir* fatDir);
int fat_getNextDirentry(fat_dir* fatDir);

#endif

