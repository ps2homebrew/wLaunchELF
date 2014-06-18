/*
 * Copyright 2010 - jimmikaelkael <jimmikaelkael@wanadoo.fr>
 * Licenced under Academic Free License version 3.0
 *
 * 10K kernel patches
 */

#include <tamtypes.h>
#include <kernel.h>

extern void kpatch_10K_elf;
extern int size_kpatch_10K_elf;

#define	ROMSEG0(vaddr)	(0xbfc00000 | vaddr)
#define	KSEG0(vaddr)	(0x80000000 | vaddr)
#define	JAL(addr)	(0x0c000000 | ((addr & 0x03ffffff) >> 2))

#define ELF_MAGIC		0x464c457f
#define ELF_PT_LOAD		1

typedef struct {
	u8	ident[16];	// struct definition for ELF object header
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

typedef struct {
	u32	type;		// struct definition for ELF program section header
	u32	offset;
	void	*vaddr;
	u32	paddr;
	u32	filesz;
	u32	memsz;
	u32	flags;
	u32	align;
} elf_pheader_t;

typedef struct {
	char fileName[10];
	u16  extinfoSize;
	int  fileSize;
} romdir_t;


static void *getRomdirFS(void) {

	register int i;
	vu32 *p_rom0 = (vu32 *)ROMSEG0(0x00000000);

	DIntr();
	ee_kmode_enter();

	// scan rom0:
	for (i=0; i<0x400000; i+=4) {
		// search for "RESET"
		if ((*p_rom0++ == 0x45534552) && (*p_rom0 == 0x00000054))
			break;
	}

	ee_kmode_exit();
	EIntr();

	return (void *)ROMSEG0(i);
}

static void *getROMfile(char *filename) {

	register u32 offset = 0;
	void *addr = NULL;
	romdir_t *romdir_fs = (romdir_t *)getRomdirFS();

	DIntr();
	ee_kmode_enter();

	while (strlen(romdir_fs->fileName) > 0) {

		if (!strcmp(romdir_fs->fileName, filename)) {
			addr = (void *)ROMSEG0(offset);
			break;
		}

		// arrange size to next 16 bytes multiple 
		if ((romdir_fs->fileSize % 0x10) == 0)
			offset += romdir_fs->fileSize;
		else
			offset += (romdir_fs->fileSize + 0x10) & 0xfffffff0;

		romdir_fs++;
	}

	ee_kmode_exit();
	EIntr();

	return addr;
}

void sysApplyKernelPatches(void) {

	u8 *romver = (u8 *)getROMfile("ROMVER");
	if (romver) {

		// Check in rom0 for PS2 with Protokernel
		DIntr();
		ee_kmode_enter();

		if ((romver[0] == '0')
		 && (romver[1] == '1')
		 && (romver[2] == '0')
		 && (romver[9] == '0')) {

			// if protokernel is unpatched
			if (_lw((u32)KSEG0(0x00002f88)) == 0x0c0015fa) {

				// we copy the patching to its placement in kernel memory
				u8 *elfptr = (u8 *)&kpatch_10K_elf;
				elf_header_t *eh = (elf_header_t *)elfptr;
				elf_pheader_t *eph = (elf_pheader_t *)&elfptr[eh->phoff];
				int i;

				for (i = 0; i < eh->phnum; i++) {
					if (eph[i].type != ELF_PT_LOAD)
						continue;

					memcpy(eph[i].vaddr, (void *)&elfptr[eph[i].offset], eph[i].filesz);

					if (eph[i].memsz > eph[i].filesz)
						memset((void *)(eph[i].vaddr + eph[i].filesz), 0, eph[i].memsz - eph[i].filesz);
				}

				// insert a JAL to our kernel code into internal ExecPS2 syscall code
				_sw(JAL(eh->entry), KSEG0(0x00002f88));
			}
		}

		ee_kmode_exit();
		EIntr();

		FlushCache(0);
		FlushCache(2);
	}
}

