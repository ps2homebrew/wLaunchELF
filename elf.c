#include "launchelf.h"

extern u8 *loader_elf;
extern int size_loader_elf;

// ELF-loading stuff
#define ELF_MAGIC		0x464c457f
#define ELF_PT_LOAD		1

typedef struct
{
	u8	ident[16];			// struct definition for ELF object header
	u16	type;
	u16	machine;
	u32	version;
	u32	entry;
	u32	phoff;
	u32	shoff;
	u32	flags;
	u16	ehsize;
	u16	phentsize;
	u16	phnum;
	u16	shentsize;
	u16	shnum;
	u16	shstrndx;
} elf_header_t;

typedef struct
{
	u32	type;				// struct definition for ELF program section header
	u32	offset;
	void	*vaddr;
	u32	paddr;
	u32	filesz;
	u32	memsz;
	u32	flags;
	u32	align;
} elf_pheader_t;

////////////////////////////////////////////////////////////////////////
// Tests for valid ELF file 
// Modified version of loader from Independence
//	(C) 2003 Marcus R. Brown <mrbrown@0xd6.org>
//
int checkELFheader(const char *path)
{
	u8 *boot_elf = (u8 *)0x1800000;
	elf_header_t *eh = (elf_header_t *)boot_elf;
	int fd, size=0, ret;
	char fullpath[MAX_PATH], tmp[MAX_PATH], *p;

	strcpy(fullpath,path);
	if(!strncmp(fullpath, "hdd0", 4)) {
		sprintf(tmp, "hdd0:%s", &path[6]);
		p = strchr(tmp, '/');
		sprintf(fullpath, "pfs0:%s", p);
		*p = 0;
		ret = fileXioMount("pfs0:", tmp, FIO_MT_RDONLY);
		if ((fd = fileXioOpen(fullpath, O_RDONLY, FIO_S_IRUSR | FIO_S_IWUSR | FIO_S_IXUSR | FIO_S_IRGRP | FIO_S_IWGRP | FIO_S_IXGRP | FIO_S_IROTH | FIO_S_IWOTH | FIO_S_IXOTH)) < 0){
			fileXioUmount("pfs0:");
			goto error;
		}
		size = fileXioLseek(fd, 0, SEEK_END);
		if (!size){
			fileXioClose(fd);
			fileXioUmount("pfs0:");
			goto error;
		}
		fileXioLseek(fd, 0, SEEK_SET);
		fileXioRead(fd, boot_elf, 52);
		fileXioClose(fd);
		fileXioUmount("pfs0:");
	}else if(!strncmp(fullpath, "mc", 2) ||
		!strncmp(fullpath, "mass", 4)) {
		if ((fd = fioOpen(fullpath, O_RDONLY)) < 0) 
			goto error;
		size = fioLseek(fd, 0, SEEK_END);
		if (!size){
			fioClose(fd);
			goto error;
		}
		fioLseek(fd, 0, SEEK_SET);
		fioRead(fd, boot_elf, 52);
		fioClose(fd);
	} else {
		return 0;
	}

	if ((_lw((u32)&eh->ident) != ELF_MAGIC) || eh->type != 2)
		goto error;
	
	return 1;
error:
	return -1;
}

////////////////////////////////////////////////////////////////////////
// loads LOADER.ELF from program memory and passes args of selected ELF
// and partition to it
// Modified version of loader from Independence
//	(C) 2003 Marcus R. Brown <mrbrown@0xd6.org>
//
void RunLoaderElf(char *filename, char *party)
{
	u8 *boot_elf = (u8 *)&loader_elf;
	elf_header_t *eh = (elf_header_t *)boot_elf;
	elf_pheader_t *eph;
	void *pdata;
	int i;
	char *argv[2];

/* Load the ELF into RAM.  */
	if (_lw((u32)&eh->ident) != ELF_MAGIC)
		while (1);

	eph = (elf_pheader_t *)(boot_elf + eh->phoff);

/* Scan through the ELF's program headers and copy them into RAM, then
									zero out any non-loaded regions.  */
	for (i = 0; i < eh->phnum; i++)
	{
		if (eph[i].type != ELF_PT_LOAD)
		continue;

		pdata = (void *)(boot_elf + eph[i].offset);
		memcpy(eph[i].vaddr, pdata, eph[i].filesz);

		if (eph[i].memsz > eph[i].filesz)
			memset(eph[i].vaddr + eph[i].filesz, 0,
					eph[i].memsz - eph[i].filesz);
	}

/* Let's go.  */
	fioExit();
	SifInitRpc(0);
	SifExitRpc();
	FlushCache(0);
	FlushCache(2);

	argv[0] = filename;
	argv[1] = party;
	
	ExecPS2((void *)eh->entry, 0, 2, argv);
}
