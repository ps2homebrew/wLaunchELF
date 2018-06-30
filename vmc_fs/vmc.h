#include <types.h>
#include <irx.h>
#include <stdio.h>
#include <sysclib.h>
#include <sysmem.h>
#include <intrman.h>
#include <iomanX.h>
#include <sys/fcntl.h>
#include <cdvdman.h>


//  Define this to enable debugging, will later support debugging levels, so only messages greater then a certain level will be displayed
//#define DEBUG 8
//For release versions of uLE, DEBUG should not be defined
//To avoid slowdown and size bloat

//  Define this to enable some basic profiling support, each function will display the time it took to run.
//  #define PROFILING


//  Memory allocation helpers.
void *malloc(int size);
void free(void *ptr);

#define TRUE 1
#define FALSE 0

#define MAX_NAME 32
#define MAX_PATH 1024


//  Vmc_fs error definitions
#define VMCFS_ERR_NO 0
#define VMCFS_ERR_INITIALIZED -1
#define VMCFS_ERR_VMC_OPEN -2
#define VMCFS_ERR_VMC_READ -3
#define VMCFS_ERR_CARD_TYPE -4
#define VMCFS_ERR_NOT_FORMATED -5
#define VMCFS_ERR_VMC_SIZE -6
#define VMCFS_ERR_NOT_MOUNT -7
#define VMCFS_ERR_MOUNT_BUSY -8
#define VMCFS_ERR_IMPLEMENTED -9


//  The devctl commands: 0x56 == V, 0x4D == M, 0x43 == C, 0x01, 0x02, ... == command number.
#define DEVCTL_VMCFS_CLEAN 0x564D4301   //  Set as free all fat cluster corresponding to a none existing object. ( Object are just marked as none existing but not removed from fat table when rmdir or remove fonctions are call. This allow to recover a deleted file. )
#define DEVCTL_VMCFS_CKFREE 0x564D4302  //  Check free space available on vmc.

//  The ioctl commands: 0x56 == V, 0x4D == M, 0x43 == C, 0x01, 0x02, ... == command number.
#define IOCTL_VMCFS_RECOVER 0x564D4303  //  Recover an object marked as none existing. ( data must be a valid path to an object in vmc file )


//  Vmc format enum
typedef enum {
    FORMAT_FULL,
    FORMAT_FAST
} Vmc_Format_Enum;


//  Memorycard type definitions
#define PSX_MEMORYCARD 0x1
#define PS2_MEMORYCARD 0x2
#define PDA_MEMORYCARD 0x3

//  Directory Entry Mode Flags
#define DF_READ 0x0001       //     Read permission.
#define DF_WRITE 0x0002      //     Write permission.
#define DF_EXECUTE 0x0004    //     Execute permission.
#define DF_PROTECTED 0x0008  //     Directory is copy protected.
#define DF_FILE 0x0010       //     Regular file.
#define DF_DIRECTORY 0x0020  //     Directory.
#define DF_040 0x0040        //     Used internally to create directories.
#define DF_080 0x0080        //     Copied?
#define DF_0100 0x0100       //     -
#define O_CREAT 0x0200       //     Used to create files.
#define DF_0400 0x0400       //     Set when files and directories are created, otherwise ignored.
#define DF_POCKETSTN 0x0800  //     PocketStation application save file.
#define DF_PSX 0x1000        //     PlayStation save file.
#define DF_HIDDEN 0x2000     //     File is hidden.
#define DF_04000 0x4000      //     -
#define DF_EXISTS 0x8000     //     This entry is in use. If this flag is clear, then the file or directory has been deleted.


//  Cluster definitions
#define MAX_CLUSTER_SIZE 0x400

#define ROOT_CLUSTER 0x00000000
#define FREE_CLUSTER 0x7FFFFFFF
#define EOF_CLUSTER 0xFFFFFFFF
#define ERROR_CLUSTER 0xFFFFFFFF
#define NOFOUND_CLUSTER 0xFFFFFFFF
#define MASK_CLUSTER 0x80000000


// The debugging functions, very very handy
#ifdef DEBUG
#define DEBUGPRINT(level, args...) \
    if (DEBUG >= level)            \
    printf(args)
#else
#define DEBUGPRINT(args...)
#endif


// Used for timing functions, use this to optimize stuff
#ifdef PROFILING
void profilerStart(iop_sys_clock_t *iopclock);
void profilerEnd(const char *function, const char *name, iop_sys_clock_t *iopclock1);
//This creates 2 variables with the names name1/name2, and starts the profiler
#define PROF_START(name)  \
    iop_sys_clock_t name; \
    profilerStart(&name);
//this takes the 2 variable names and ends the profiler, printing the time taken
#define PROF_END(name) \
    profilerEnd(__func__, #name, &name);
#else
//define away the profiler functions
#define PROF_START(args) ;
#define PROF_END(args) ;
#endif


//  Global Structures Defintions

//  General data struct shared by both files  /  folders that we have opened
struct gen_privdata
{
    int fd;
    unsigned int indir_fat_clusters[32];
    unsigned int first_allocatable;
    unsigned int last_allocatable;
    unsigned char dirent_page;
};

//  the structure used by files that we have opened
struct file_privdata
{
    struct gen_privdata gendata;
    unsigned int file_startcluster;
    unsigned int file_length;
    unsigned int file_position;
    unsigned int file_cluster;
    unsigned int cluster_offset;
    unsigned int file_dirpage;
};

//  the structure used by directories that we have opened
struct dir_privdata
{
    struct gen_privdata gendata;
    unsigned int dir_number;   //  first or second entry in the cluster?
    unsigned int dir_cluster;  //  what cluster we are currently reading directory entries from
    unsigned int dir_length;   //  the length of the directory
};

//  date  /  time descriptor
typedef struct
{
    unsigned char unused0;
    unsigned char sec;
    unsigned char min;
    unsigned char hour;
    unsigned char day;
    unsigned char month;
    unsigned short year;
} vmc_datetime;

//  the structure of a directory entry
struct direntry
{
    unsigned short mode;         //  See directory mode definitions.
    unsigned char unused0[2];    //  -
    unsigned int length;         //  Length in bytes if a file, or entries if a directory.
    vmc_datetime created;        //  created time.
    unsigned int cluster;        //  First cluster of the file, or 0xFFFFFFFF for an empty file. In "." entries it's the first cluster of the parent directory relative to first_allocatable.
    unsigned int dir_entry;      //  Only in "." entries. Entry of this directory in its parent's directory.
    vmc_datetime modified;       //  Modification time.
    unsigned int attr;           //  User defined attribute.
    unsigned char unused1[28];   //  -
    char name[32];               //  Zero terminated name for this directory entry.
    unsigned char unused2[416];  //  -
};

//  A structure containing all of the information about the superblock on a memory card.
struct superblock
{
    unsigned char magic[40];
    unsigned short page_size;
    unsigned short pages_per_cluster;
    unsigned short pages_per_block;
    unsigned short unused0;  //  always 0xFF00
    unsigned int clusters_per_card;
    unsigned int first_allocatable;
    unsigned int last_allocatable;
    unsigned int root_cluster;   //  must be 0
    unsigned int backup_block1;  //  1023
    unsigned int backup_block2;  //  1024
    unsigned char unused1[8];    //  unused  /  who knows what it is
    unsigned int indir_fat_clusters[32];
    unsigned int bad_block_list[32];
    unsigned char mc_type;
    unsigned char mc_flag;
    unsigned short unused2;     //  zero
    unsigned int cluster_size;  //  1024 always, 0x400
    unsigned int unused3;       //  0x100
    unsigned int size_in_megs;  //  size in megabytes
    unsigned int unused4;       //  0xffffffff
    unsigned char unused5[12];  //  zero
    unsigned int max_used;      //  97%of total clusters
    unsigned char unused6[8];   //  zero
    unsigned int unused7;       //  0xffffffff
};

//  General vmc image structure
struct global_vmc
{
    int fd;                                           //  global descriptor
    struct superblock header;                         //  superblock header
    int formated;                                     //  card is formated
    int ecc_flag;                                     //  ecc data found in vmc file
    unsigned int card_size;                           //  vmc file size
    unsigned int total_pages;                         //  total number of pages in the vmc file
    unsigned int cluster_size;                        //  size in byte of a cluster
    unsigned short erase_byte;                        //  erased blocks have all bits set to 0x0 or 0xFF
    unsigned int last_idc;                            //  last indirect cluster
    unsigned int last_cluster;                        //  last cluster
    unsigned int indirect_cluster[MAX_CLUSTER_SIZE];  //  indirect fat cluster
    unsigned int fat_cluster[MAX_CLUSTER_SIZE];       //  fat cluster
    unsigned int last_free_cluster;                   //  adress of the last free cluster found when getting free cluster
};


extern struct global_vmc g_Vmc_Image[2];
extern int g_Vmc_Format_Mode;
extern int g_Vmc_Remove_Flag;
extern int g_Vmc_Initialized;


//  vmc_fs.c
int Vmc_Initialize(iop_device_t *driver);
int Vmc_Deinitialize(iop_device_t *driver);

// vmc_io.c
int Vmc_Format(iop_file_t *, const char *dev, const char *blockdev, void *arg, int arglen);
int Vmc_Open(iop_file_t *f, const char *name, int flags, int mode);
int Vmc_Close(iop_file_t *f);
int Vmc_Read(iop_file_t *f, void *buffer, int size);
int Vmc_Write(iop_file_t *f, void *buffer, int size);
int Vmc_Lseek(iop_file_t *f, int offset, int whence);
int Vmc_Ioctl(iop_file_t *f, int cmd, void *data);
int Vmc_Remove(iop_file_t *f, const char *path);
int Vmc_Mkdir(iop_file_t *f, const char *path1, int mode);
int Vmc_Rmdir(iop_file_t *f, const char *path1);
int Vmc_Dopen(iop_file_t *f, const char *path);
int Vmc_Dclose(iop_file_t *f);
int Vmc_Dread(iop_file_t *f, iox_dirent_t *buf);
int Vmc_Getstat(iop_file_t *f, const char *path, iox_stat_t *stat);
int Vmc_Chstat(iop_file_t *f, const char *path, iox_stat_t *stat, unsigned int statmask);
int Vmc_Rename(iop_file_t *f, const char *path, const char *new_name);
int Vmc_Chdir(iop_file_t *f, const char *path);
int Vmc_Sync(iop_file_t *f, const char *device, int flag);
int Vmc_Mount(iop_file_t *f, const char *fsname, const char *devname, int flag, void *arg, int arg_len);
int Vmc_Umount(iop_file_t *f, const char *fsname);
s64 Vmc_Lseek64(iop_file_t *f, s64 offset, int whence);
int Vmc_Devctl(iop_file_t *f, const char *path, int cmd, void *arg, unsigned int arglen, void *buf, unsigned int buflen);
int Vmc_Symlink(iop_file_t *f, const char *old, const char *new);
int Vmc_Readlink(iop_file_t *f, const char *path, char *buf, unsigned int buf_len);
int Vmc_Ioctl2(iop_file_t *f, int cmd, void *arg, unsigned int arglen, void *buf, unsigned int buflen);
int Vmc_Recover(int unit, const char *path1);
unsigned int Vmc_Checkfree(int unit);
int Vmc_Clean(int unit);

//  mcfat.c
typedef enum {
    FAT_VALUE,
    FAT_MASK
} GetFat_Mode;

typedef enum {
    FAT_RESET,
    FAT_SET
} SetFat_Mode;

unsigned int getFatEntry(int fd, unsigned int cluster, unsigned int *indir_fat_clusters, GetFat_Mode Mode);
unsigned int setFatEntry(int fd, unsigned int cluster, unsigned int value, unsigned int *indir_fat_clusters, SetFat_Mode Mode);


//  ps2.c
int eraseBlock(int fd, unsigned int block);
int writePage(int fd, u8 *page, unsigned int pagenum);
int writeCluster(int fd, u8 *cluster, unsigned int clusternum);
int writeClusterPart(int fd, u8 *cluster, unsigned int clusternum, int cluster_offset, int size);
int readPage(int fd, u8 *page, unsigned int pagenum);
int readCluster(int fd, u8 *cluster, unsigned int clusternum);


//  misc.c
unsigned int getDirentryFromPath(struct direntry *retval, const char *path, struct gen_privdata *gendata, int unit);
unsigned int addObject(struct gen_privdata *gendata, unsigned int parentcluster, struct direntry *parent, struct direntry *dirent, int unit);
void removeObject(struct gen_privdata *gendata, unsigned int dirent_cluster, struct direntry *dirent, int unit);
unsigned int getFreeCluster(struct gen_privdata *gendata, int unit);
int getPs2Time(vmc_datetime *tm);
int setDefaultSpec(int unit);
void buildECC(int unit, const u8 *Page_Data, u8 *ECC_Data);
