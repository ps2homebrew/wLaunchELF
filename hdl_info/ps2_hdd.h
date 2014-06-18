/*
 * ps2_hdd.h
 * $Id: ps2_hdd.h,v 1.5 2005/07/10 21:06:48 bobi Exp $
 *
 * borrowed from ps2fdisk
 */

#ifndef _PS2_HDD_H_
#define _PS2_HDD_H_

typedef unsigned char  u_char;
typedef unsigned int   u_int;
typedef unsigned short u_short;
typedef unsigned long  u_long;

/* Various PS2 partition constants */
#define PS2_PARTITION_MAGIC	"APA"	/* "APA\0" */
#define PS2_PART_IDMAX		32
#define PS2_PART_NAMEMAX	128
#define PS2_PART_MAXSUB		64	/* Maximum # of sub-partitions */
#define PS2_PART_FLAG_SUB	0x0001	/* Is partition a sub-partition? */
#define PS2_MBR_VERSION		2	/* Current MBR version */

/* Partition types */
#define PS2_MBR_PARTITION	0x0001
#define PS2_SWAP_PARTITION	0x0082
#define PS2_LINUX_PARTITION	0x0083
#define PS2_GAME_PARTITION	0x0100

/* Date/time descriptor used in on-disk partition header */
typedef struct ps2fs_datetime_type
{
  u_char unused;
  u_char sec;
  u_char min;
  u_char hour;
  u_char day;
  u_char month;
  u_short year;
} ps2fs_datetime_t;

/* On-disk partition header for a partition */
typedef struct ps2_partition_header_type
{
  u_long checksum;	/* Sum of all 256 words, assuming checksum==0 */
  u_char magic [4];	/* PS2_PARTITION_MAGIC */
  u_long next;	/* Sector address of next partition */
  u_long prev;	/* Sector address of previous partition */
  char id [PS2_PART_IDMAX];
  char unknown1 [16];
  u_long start;	/* Sector address of this partition */
  u_long length;	/* Sector count */
  u_short type;
  u_short flags;	/* PS2_PART_FLAG_* */
  u_long nsub;	/* No. of sub-partitions (stored in main partition) */
  ps2fs_datetime_t created;
  u_long main;	/* For sub-partitions, main partition sector address */
  u_long number;	/* For sub-partitions, sub-partition number */
  u_short unknown2;
  char unknown3 [30];
  char name [PS2_PART_NAMEMAX];
  struct
  {
    char magic [32];	/* Copyright message in MBR */
    char unknown_0x02;
    char unknown1 [7];
    ps2fs_datetime_t created; /* Same as for the partition, it seems*/
    u_long data_start;	/* Some sort of MBR data; position in sectors*/
    u_long data_len;	/* Length also in sectors */
    char unknown2 [200];
  } mbr;
  struct
  {		/* Sub-partition data */
    u_long start;/* Sector address */
    u_long length;/* Sector count */
  } subs [PS2_PART_MAXSUB];
} ps2_partition_header_t;

#endif /* _PS2_HDD_H_ */
