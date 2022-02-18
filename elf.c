//--------------------------------------------------------------
// File name:    elf.c
//--------------------------------------------------------------
#include "launchelf.h"

#define MAX_PATH 1025

extern u8 loader_elf[];
extern int size_loader_elf;

// ELF-loading stuff
#define ELF_MAGIC   0x464c457f
#define ELF_PT_LOAD 1

//------------------------------
typedef struct
{
    u8 ident[16];  // struct definition for ELF object header
    u16 type;
    u16 machine;
    u32 version;
    u32 entry;
    u32 phoff;
    u32 shoff;
    u32 flags;
    u16 ehsize;
    u16 phentsize;
    u16 phnum;
    u16 shentsize;
    u16 shnum;
    u16 shstrndx;
} elf_header_t;
//------------------------------
typedef struct
{
    u32 type;  // struct definition for ELF program section header
    u32 offset;
    void *vaddr;
    u32 paddr;
    u32 filesz;
    u32 memsz;
    u32 flags;
    u32 align;
} elf_pheader_t;
//--------------------------------------------------------------
// End of data declarations
//--------------------------------------------------------------
// Start of function code
//--------------------------------------------------------------
// checkELFheader Tests for valid ELF file
// Modified version of loader from Independence
//	(C) 2003 Marcus R. Brown <mrbrown@0xd6.org>
//--------------------------------------------------------------
int checkELFheader(char *path)
{
    elf_header_t elf_head;
    u8 *boot_elf = (u8 *)&elf_head;
    elf_header_t *eh = (elf_header_t *)boot_elf;
    int fd, size = 0, ret;
    char fullpath[MAX_PATH], tmp[MAX_PATH], *p;

    strcpy(fullpath, path);
    if (!strncmp(fullpath, "mc", 2) || !strncmp(fullpath, "vmc", 3) || !strncmp(fullpath, "rom", 3) || !strncmp(fullpath, "cdrom", 5) || !strncmp(fullpath, "cdfs", 4)) {
        ;  // fullpath is already correct
    } else if (!strncmp(fullpath, "hdd0:", 5)) {
        p = &path[5];
        if (*p == '/')
            p++;
        sprintf(tmp, "hdd0:%s", p);
        p = strchr(tmp, '/');
        sprintf(fullpath, "pfs0:%s", p);
        *p = 0;
        if ((ret = mountParty(tmp)) < 0)
            goto error;
        fullpath[3] += ret;
    } else if (!strncmp(fullpath, "dvr_hdd0:", 9)) {
        p = &path[9];
        if (*p == '/')
            p++;
        sprintf(tmp, "dvr_hdd0:%s", p);
        p = strchr(tmp, '/');
        sprintf(fullpath, "dvr_pfs0:%s", p);
        *p = 0;
        if ((ret = mountDVRPParty(tmp)) < 0)
            goto error;
        fullpath[7] += ret;
    } else if (!strncmp(fullpath, "mass", 4)) {
        char *pathSep;

        pathSep = strchr(path, '/');
        if (pathSep && (pathSep - path < 7) && pathSep[-1] == ':')
            strcpy(fullpath + (pathSep - path), pathSep + 1);
    } else if (!strncmp(fullpath, "host:", 5)) {
        if (path[5] == '/')
            strcpy(fullpath + 5, path + 6);
    } else {
        return 0;  // return 0 for unrecognized device
    }
    if ((fd = genOpen(fullpath, O_RDONLY)) < 0)
        goto error;
    size = genLseek(fd, 0, SEEK_END);
    if (!size) {
        genClose(fd);
        goto error;
    }
    genLseek(fd, 0, SEEK_SET);
    genRead(fd, boot_elf, sizeof(elf_header_t));
    genClose(fd);

    if ((_lw((u32)&eh->ident) != ELF_MAGIC) || eh->type != 2)
        goto error;

    return 1;  // return 1 for successful check
error:
    return -1;  // return -1 for failed check
}
//------------------------------
// End of func:  int checkELFheader(const char *path)
//--------------------------------------------------------------
// RunLoaderElf loads LOADER.ELF from program memory and passes
// args of selected ELF and partition to it
// Modified version of loader from Independence
//	(C) 2003 Marcus R. Brown <mrbrown@0xd6.org>
//------------------------------
void RunLoaderElf(char *filename, char *party)
{
    u8 *boot_elf;
    elf_header_t *eh;
    elf_pheader_t *eph;
    void *pdata;
    int i;
    char *argv[2], bootpath[256];

    if ((!strncmp(party, "hdd0:", 5)) && (!strncmp(filename, "pfs0:", 5))) {
        if (0 > fileXioMount("pfs0:", party, FIO_MT_RDONLY)) {
            // Some error occurred, it could be due to something else having used pfs0
            unmountParty(0);  // So we try unmounting pfs0, to try again
            if (0 > fileXioMount("pfs0:", party, FIO_MT_RDONLY))
                return;  // If it still fails, we have to give up...
        }

        // If a path to a file on PFS is specified, change it to the standard format.
        // hdd0:partition:pfs:path/to/file
        if (strncmp(filename, "pfs0:", 5) == 0) {
            sprintf(bootpath, "%s:pfs:%s", party, &filename[5]);
        } else {
            sprintf(bootpath, "%s:%s", party, filename);
        }

        argv[0] = filename;
        argv[1] = bootpath;
    } else if ((!strncmp(party, "dvr_hdd0:", 9)) && (!strncmp(filename, "dvr_pfs0:", 9))) {
        if (0 > fileXioMount("dvr_pfs0:", party, FIO_MT_RDONLY)) {
            // Some error occurred, it could be due to something else having used pfs0
            unmountDVRPParty(0);  // So we try unmounting pfs0, to try again
            if (0 > fileXioMount("dvr_pfs0:", party, FIO_MT_RDONLY))
                return;  // If it still fails, we have to give up...
        }

        // If a path to a file on PFS is specified, change it to the standard format.
        // dvr_hdd0:partition:pfs:path/to/file
        if (strncmp(filename, "dvr_pfs0:", 9) == 0) {
            sprintf(bootpath, "%s:pfs:%s", party, &filename[9]);
        } else {
            sprintf(bootpath, "%s:%s", party, filename);
        }

        argv[0] = filename;
        argv[1] = bootpath;
    } else {
        argv[0] = filename;
        argv[1] = filename;
    }

    /* NB: LOADER.ELF is embedded  */
    boot_elf = (u8 *)loader_elf;
    eh = (elf_header_t *)boot_elf;
    if (_lw((u32)&eh->ident) != ELF_MAGIC)
        asm volatile("break\n");

    eph = (elf_pheader_t *)(boot_elf + eh->phoff);

    /* Scan through the ELF's program headers and copy them into RAM, then
                                    zero out any non-loaded regions.  */
    for (i = 0; i < eh->phnum; i++) {
        if (eph[i].type != ELF_PT_LOAD)
            continue;

        pdata = (void *)(boot_elf + eph[i].offset);
        memcpy(eph[i].vaddr, pdata, eph[i].filesz);

        if (eph[i].memsz > eph[i].filesz)
            memset(eph[i].vaddr + eph[i].filesz, 0,
                   eph[i].memsz - eph[i].filesz);
    }

    /* Let's go.  */
    SifExitRpc();
    FlushCache(0);
    FlushCache(2);

    ExecPS2((void *)eh->entry, NULL, 2, argv);
}
//------------------------------
// End of func:  void RunLoaderElf(char *filename, char *party)
//--------------------------------------------------------------
// End of file:  elf.c
//--------------------------------------------------------------
