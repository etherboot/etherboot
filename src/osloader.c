/**************************************************************************
OS loader

Author: Markus Gutschke (gutschk@math.uni-muenster.de)
  Date: Sep/95
Modifications: Ken Yap (for Etherboot/16)
  Doug Ambrisko (ELF and a.out support)
  Klaus Espenlaub (rewrote ELF and a.out (did it really work before?) support,
      added ELF Multiboot images).  Someone should merge the ELF and a.out
      loaders, as most of the code is now identical.  Maybe even NBI could be
      rewritten and merged into the generic loading framework.  This should
      save quite a few bytes of code if you have selected more than one format.
  Ken Yap (Jan 2001)
      Added support for linear entry addresses in tagged images,
      which allows a more efficient protected mode call instead of
      going to real mode and back. Also means entry addresses > 1 MB can
      be called.  Conditional on the LINEAR_EXEC_ADDR bit.
      Added support for Etherboot extension calls. Conditional on the
      TAGGED_PROGRAM_RETURNS bit. Implies LINEAR_EXEC_ADDR.
      Added support for non-MULTIBOOT ELF which also supports Etherboot
      extension calls. Conditional on the ELF_PROGRAM_RETURNS bit.

**************************************************************************/

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 */

#include "etherboot.h"

/* bootinfo */
#define BOOTINFO_VERSION 1
#define NODEV           (-1)    /* non-existent device */
#define PAGE_SHIFT      12              /* LOG2(PAGE_SIZE) */
#define PAGE_SIZE       (1<<PAGE_SHIFT) /* bytes/page */
#define PAGE_MASK       (PAGE_SIZE-1)
#define N_BIOS_GEOM     8

struct bootinfo {
        unsigned int            bi_version;
        const unsigned char     *bi_kernelname;
        struct nfs_diskless     *bi_nfs_diskless;
                                /* End of fields that are always present. */
#define bi_endcommon            bi_n_bios_used
        unsigned int            bi_n_bios_used;
        unsigned long           bi_bios_geom[N_BIOS_GEOM];
        unsigned int            bi_size;
        unsigned char           bi_memsizes_valid;
        unsigned char           bi_pad[3];
        unsigned long           bi_basemem;
        unsigned long           bi_extmem;
        unsigned long           bi_symtab;
        unsigned long           bi_esymtab;
#ifdef  IMAGE_FREEBSD
	/* Note that these are in the FreeBSD headers but were not here... */
	unsigned long           bi_kernend;		/* end of kernel space */
	unsigned long           bi_envp;		/* environment */
	unsigned long           bi_modulep;		/* preloaded modules */
#endif
};

/* a.out */
struct exec {
     unsigned long      a_midmag;       /* flags<<26 | mid<<16 | magic */
     unsigned long      a_text;         /* text segment size */
     unsigned long      a_data;         /* initialized data size */
     unsigned long      a_bss;          /* uninitialized data size */
     unsigned long      a_syms;         /* symbol table size */
     unsigned long      a_entry;        /* entry point */
     unsigned long      a_trsize;       /* text relocation size */
     unsigned long      a_drsize;       /* data relocation size */
};

/* ELF */
#define EI_NIDENT	16	/* Size of e_ident array. */

/* Values for e_type. */
#define ET_NONE		0	/* No file type */
#define ET_REL		1	/* Relocatable file */
#define ET_EXEC		2	/* Executable file */
#define ET_DYN		3	/* Shared object file */
#define ET_CORE		4	/* Core file */

/* Values for e_machine (incomplete). */
#define EM_386		3	/* Intel 80386 */
#define EM_486		6	/* Intel i486 */

/* Values for p_type. */
#define PT_NULL		0	/* Unused entry. */
#define PT_LOAD		1	/* Loadable segment. */
#define PT_DYNAMIC	2	/* Dynamic linking information segment. */
#define PT_INTERP	3	/* Pathname of interpreter. */
#define PT_NOTE		4	/* Auxiliary information. */
#define PT_SHLIB	5	/* Reserved (not used). */
#define PT_PHDR		6	/* Location of program header itself. */

/* Values for p_flags. */
#define PF_X		0x1	/* Executable. */
#define PF_W		0x2	/* Writable. */
#define PF_R		0x4	/* Readable. */

/*
 * ELF definitions common to all 32-bit architectures.
 */

typedef unsigned int	Elf32_Addr;
typedef unsigned short	Elf32_Half;
typedef unsigned int	Elf32_Off;
typedef int		Elf32_Sword;
typedef unsigned int	Elf32_Word;
typedef unsigned int	Elf32_Size;

/*
 * ELF header.
 */
typedef struct {
	unsigned char	e_ident[EI_NIDENT];	/* File identification. */
	Elf32_Half	e_type;		/* File type. */
	Elf32_Half	e_machine;	/* Machine architecture. */
	Elf32_Word	e_version;	/* ELF format version. */
	Elf32_Addr	e_entry;	/* Entry point. */
	Elf32_Off	e_phoff;	/* Program header file offset. */
	Elf32_Off	e_shoff;	/* Section header file offset. */
	Elf32_Word	e_flags;	/* Architecture-specific flags. */
	Elf32_Half	e_ehsize;	/* Size of ELF header in bytes. */
	Elf32_Half	e_phentsize;	/* Size of program header entry. */
	Elf32_Half	e_phnum;	/* Number of program header entries. */
	Elf32_Half	e_shentsize;	/* Size of section header entry. */
	Elf32_Half	e_shnum;	/* Number of section header entries. */
	Elf32_Half	e_shstrndx;	/* Section name strings section. */
} Elf32_Ehdr;

#define	ELF_PROGRAM_RETURNS_BIT	0x8000000	/* e_flags bit 31 */

/*
 * Program header.
 */
typedef struct {
	Elf32_Word	p_type;		/* Entry type. */
	Elf32_Off	p_offset;	/* File offset of contents. */
	Elf32_Addr	p_vaddr;	/* Virtual address (not used). */
	Elf32_Addr	p_paddr;	/* Physical address. */
	Elf32_Size	p_filesz;	/* Size of contents in file. */
	Elf32_Size	p_memsz;	/* Size of contents in memory. */
	Elf32_Word	p_flags;	/* Access permission flags. */
	Elf32_Size	p_align;	/* Alignment in memory and file. */
} Elf32_Phdr;

#ifdef  IMAGE_FREEBSD
/*
 * FreeBSD has this rather strange "feature" of its design.
 * At some point in its evolution, FreeBSD started to rely
 * externally on private/static/debug internal symbol information.
 * That is, some of the interfaces that software uses to access
 * and work with the FreeBSD kernel are made available not
 * via the shared library symbol information (the .DYNAMIC section)
 * but rather the debug symbols.  This means that any symbol, not
 * just publicly defined symbols can be (and are) used by system
 * tools to make the system work.  (such as top, swapinfo, swapon,
 * etc)
 *
 * Even worse, however, is the fact that standard ELF loaders do
 * not know how to load the symbols since they are not within
 * an ELF PT_LOAD section.  The kernel needs these symbols to
 * operate so the following changes/additions to the boot
 * loading of EtherBoot have been made to get the kernel to load.
 * All of the changes are within IMAGE_FREEBSD such that the
 * extra/changed code only compiles when FREEBSD support is
 * enabled.
 */

/*
 * Section header for FreeBSD (debug symbol kludge!) support
 */
typedef struct {
	Elf32_Word	sh_name;	/* Section name (index into the
					   section header string table). */
	Elf32_Word	sh_type;	/* Section type. */
	Elf32_Word	sh_flags;	/* Section flags. */
	Elf32_Addr	sh_addr;	/* Address in memory image. */
	Elf32_Off	sh_offset;	/* Offset in file. */
	Elf32_Size	sh_size;	/* Size in bytes. */
	Elf32_Word	sh_link;	/* Index of a related section. */
	Elf32_Word	sh_info;	/* Depends on section type. */
	Elf32_Size	sh_addralign;	/* Alignment in bytes. */
	Elf32_Size	sh_entsize;	/* Size of each entry in section. */
} Elf32_Shdr;

/* sh_type */
#define SHT_SYMTAB	2		/* symbol table section */
#define SHT_STRTAB	3		/* string table section */

/*
 * Module information subtypes (for the metadata that we need to build)
 */
#define MODINFO_END		0x0000		/* End of list */
#define MODINFO_NAME		0x0001		/* Name of module (string) */
#define MODINFO_TYPE		0x0002		/* Type of module (string) */
#define MODINFO_METADATA	0x8000		/* Module-specfic */

#define MODINFOMD_SSYM		0x0003		/* start of symbols */
#define MODINFOMD_ESYM		0x0004		/* end of symbols */

#endif	/* IMAGE_FREEBSD */

/* The structure of a Multiboot 0.6 parameter block.  */
struct multiboot_info {
	unsigned int flags;
	unsigned int memlower;
	unsigned int memupper;
	unsigned int bootdev;
	void *cmdline;
};

/* Define some structures used by tftp loader */

struct imgheader
{
	unsigned long magic;
	unsigned long length;			/* and flags */
	union
	{
		struct { unsigned short bx, ds; } segoff;
		unsigned long location;
	} u;
	unsigned long execaddr;
};

union infoblock
{
	unsigned char dummy[512];		/* Establishes the min size */
	unsigned short s[256];
	unsigned long l[128];
	struct imgheader img;
	struct exec head;
	struct bootinfo bsdinfo;
	struct multiboot_info mbinfo;
	Elf32_Ehdr elf32;
};

struct segheader
{
	unsigned char length;
	unsigned char vendortag;
	unsigned char reserved;
	unsigned char flags;
	unsigned long loadaddr;
	unsigned long imglength;
	unsigned long memlength;
};

static enum {
	Unknown, Tagged, Aout, Elf, Aout_FreeBSD, Elf_FreeBSD
} image_type = Unknown;

/* The following are static because os_download is called for each block
   and we need to retain info across calls */

static Address curaddr;

#ifdef	TAGGED_IMAGE
/* Keep all context about loaded image in one place */
static struct tagged_context
{
	struct imgheader	img;			/* copy of header */
	unsigned long		linlocation;		/* addr of header */
	unsigned long		last0, last1;		/* of prev segment */
	unsigned long		segaddr, seglen;	/* of current segment */
	unsigned char		segflags;
} tctx;

#define	TAGGED_PROGRAM_RETURNS	(tctx.img.length & 0x00000100)	/* bit 8 */
#define	LINEAR_EXEC_ADDR	(tctx.img.length & 0x80000000)	/* bit 31 */

#endif

#if	defined(AOUT_IMAGE) || defined(ELF_IMAGE)
static union infoblock	info;
static int segment;		/* current segment number, -1 for none */
static unsigned int loc;	/* start offset of current block */
static unsigned int skip;	/* padding to be skipped to current segment */
static unsigned int toread;	/* remaining data to be read in the segment */
#endif

#ifdef	ELF_IMAGE
static Elf32_Phdr *phdr;

#ifdef	IMAGE_FREEBSD
static Elf32_Shdr *shdr;	/* To support the FreeBSD kludge! */
static Address symtab_load;
static Address symstr_load;
static int symtabindex;
static int symstrindex;
#endif

#endif

#ifdef  IMAGE_FREEBSD
static unsigned int off;
#endif

static struct ebinfo		loaderinfo = {
	VERSION_MAJOR, VERSION_MINOR,
	0
};

#ifdef	TAGGED_IMAGE
static int tagged_download(unsigned char *data, unsigned int len)
{
	int	i,j;

	do {
		while (tctx.seglen == 0) {
			struct segheader	sh;
			if (tctx.segflags & 0x04) {

#ifdef	SIZEINDICATOR
				printf("K ");
#endif
				printf("done\n");
#ifdef	DELIMITERLINES
				for (j=0; j<80; j++) putchar('=');
				putchar('\n');
#endif
				if (!TAGGED_PROGRAM_RETURNS)
					cleanup();
				if (LINEAR_EXEC_ADDR) {
					int result, (*entry)(struct ebinfo *, long, struct bootpd_t *);
					/* no gateA20_unset for PM call */
					entry = (int (*)())tctx.img.execaddr;
					result = (*entry)(&loaderinfo, tctx.linlocation, BOOTP_DATA_ADDR);
					printf("Secondary program returned %d\n", result);
					if (!TAGGED_PROGRAM_RETURNS) {
						/* We shouldn't have returned */
						result = -2;
					}
					longjmp(restart_etherboot, result);
						
				} else {
					gateA20_unset();
					xstart(tctx.img.execaddr, tctx.img.u.location, (char *)BOOTP_DATA_ADDR);
					longjmp(restart_etherboot, -2);
				}
			}
			sh = *((struct segheader *)tctx.segaddr);
			tctx.seglen = sh.imglength;
			if ((tctx.segflags = sh.flags & 0x03) == 0)
				curaddr = sh.loadaddr;
			else if (tctx.segflags == 0x01)
				curaddr = tctx.last1 + sh.loadaddr;
			else if (tctx.segflags == 0x02)
				curaddr = (Address)(memsize() * 1024L
						    + 0x100000L)
					- sh.loadaddr;
			else
				curaddr = tctx.last0 - sh.loadaddr;
			tctx.last1 = (tctx.last0 = curaddr) + sh.memlength;
			tctx.segflags = sh.flags;
			tctx.segaddr += ((sh.length & 0x0F) << 2)
				+ ((sh.length & 0xF0) >> 2);
		}
		i = (tctx.seglen > len) ? len : tctx.seglen;
		memcpy((void *)curaddr, data, i);
		tctx.seglen -= i;
		curaddr += i;
		len -= i;
		data += i;
	} while (len > 0);
	return (1);
}
#endif

#ifdef	AOUT_IMAGE
static int aout_download(unsigned char *data, unsigned int len)
{
	unsigned int offset;	/* working offset in the current data block */

	if (len == 0) {
		int	j;
		void	(*entry)();

aout_startkernel:
		entry = (void *)info.head.a_entry;
		printf("done\n");
#ifdef	DELIMITERLINES
		for (j=0; j<80; j++) putchar('=');
		putchar('\n');
#endif
		cleanup();

#ifdef	IMAGE_FREEBSD
		if (image_type == Aout_FreeBSD) {
			memset(&info.bsdinfo, 0, sizeof(info.bsdinfo));
			info.bsdinfo.bi_basemem = basememsize();
			info.bsdinfo.bi_extmem = memsize();
			info.bsdinfo.bi_memsizes_valid = 1;
			info.bsdinfo.bi_version = BOOTINFO_VERSION;
			info.bsdinfo.bi_kernelname = KERNEL_BUF;
			info.bsdinfo.bi_nfs_diskless = NULL;
			info.bsdinfo.bi_size = sizeof(info.bsdinfo);
			(*entry)(freebsd_howto, NODEV, 0, 0, 0, &info.bsdinfo, 0, 0, 0);
			longjmp(restart_etherboot, -2);
		}
#endif
		printf("unexpected a.out variant\n");
		longjmp(restart_etherboot, -2);
	}
	offset = 0;
	do {
		if (segment != -1) {
			if (skip) {
				if (skip >= len - offset) {
					skip -= len - offset;
					break;
				}
				offset += skip;
				skip = 0;
			}

			if (toread) {
				if (toread >= len - offset) {
					memcpy((void *)curaddr, data+offset,
						len - offset);
					curaddr += len - offset;
					toread -= len - offset;
					break;
				}
				memcpy((void *)curaddr, data+offset, toread);
				offset += toread;
				toread = 0;
			}
		}

		/* Data left, but current segment finished - look for the next
		 * segment.  This is quite simple for a.out files.  */
		segment++;
		switch (segment) {
		case 0:
			/* read text */
			curaddr = info.head.a_entry;
			skip = 4096;
			toread = info.head.a_text;
			break;
		case 1:
			/* read data */
			/* skip and curaddr may be wrong, but I couldn't find
			 * examples where this failed.  There is no reasonable
			 * documentation for a.out available.  */
			skip = ((curaddr + 4095) & ~4095) - curaddr;
			curaddr = (curaddr + 4095) & ~4095;
			toread = info.head.a_data;
			break;
		case 2:
			/* initialize bss and start kernel */
			curaddr = (curaddr + 4095) & ~4095;
			skip = 0;
			toread = 0;
			memset((void *)curaddr, '\0', info.head.a_bss);
			goto aout_startkernel;
		default:
			break;
		}
	} while (offset < len);

	loc += len;
	return 1;
}
#endif

#ifdef	ELF_IMAGE
static int elf_download(unsigned char *data, unsigned int len)
{
	unsigned int offset;	/* working offset in the current data block */
	int i;

	if (len == 0) {
		int	j;
		void (*entry)();
#ifdef	IMAGE_MULTIBOOT
		unsigned char cmdline[512], *c;
#endif

elf_startkernel:
		entry = (void *)info.elf32.e_entry;
		printf("done\n");
#ifdef	DELIMITERLINES
		for (j=0; j<80; j++)
			putchar('=');
		putchar('\n');
#endif
#ifdef IMAGE_FREEBSD
		if (image_type == Elf_FreeBSD) {
			cleanup();
			memset(&info.bsdinfo, 0, sizeof(info.bsdinfo));
			info.bsdinfo.bi_basemem = basememsize();
			info.bsdinfo.bi_extmem = memsize();
			info.bsdinfo.bi_memsizes_valid = 1;
			info.bsdinfo.bi_version = BOOTINFO_VERSION;
			info.bsdinfo.bi_kernelname = KERNEL_BUF;
			info.bsdinfo.bi_nfs_diskless = NULL;
			info.bsdinfo.bi_size = sizeof(info.bsdinfo);

			/* Check if we have symbols loaded, and if so,
			 * made the meta_data needed to pass those to
			 * the kernel. */
			if ((symtab_load !=0) && (symstr_load != 0))
			{
				unsigned long *t;

				info.bsdinfo.bi_symtab = symtab_load;

				/* End of symbols (long aligned...) */
				/* Assumes size of long is a power of 2... */
				info.bsdinfo.bi_esymtab = (symstr_load +
					sizeof(long) +
					*((long *)symstr_load) +
					sizeof(long) - 1) & ~(sizeof(long) - 1);

				/* Where we will build the meta data... */
				t = (unsigned long *)info.bsdinfo.bi_esymtab;

#ifdef	DEBUG_ELF
				printf("Metadata at %X\n",t);
#endif

				/* Set up the pointer to the memory... */
				info.bsdinfo.bi_modulep = (unsigned long)t;
				
				/* The metadata structure is an array of 32-bit
				 * words where we store some information about the
				 * system.  This is critical, as FreeBSD now looks
				 * only for the metadata for the extended symbol
				 * information rather than in the bootinfo.
				 */
				/* First, do the kernel name and the kernel type */
				/* Note that this assumed x86 byte order... */

				/* 'kernel\0\0' */
				*t++=MODINFO_NAME; *t++= 7; *t++=0x6E72656B; *t++=0x00006C65;

				/* 'elf kernel\0\0' */
				*t++=MODINFO_TYPE; *t++=11; *t++=0x20666C65; *t++=0x6E72656B; *t++ = 0x00006C65;

				/* Now the symbol start/end - note that they are
				 * here in local/physical address - the Kernel
				 * boot process will relocate the addresses. */
				*t++=MODINFOMD_SSYM | MODINFO_METADATA; *t++=sizeof(*t); *t++=info.bsdinfo.bi_symtab;
				*t++=MODINFOMD_ESYM | MODINFO_METADATA; *t++=sizeof(*t); *t++=info.bsdinfo.bi_esymtab;

				*t++=MODINFO_END; *t++=0; /* end of metadata */

				/* Since we have symbols we need to make
				 * sure that the kernel knows its own end
				 * of memory...  It is not _end but after
				 * the symbols and the metadata... */
				info.bsdinfo.bi_kernend = (unsigned long)t;

				/* Signal locore.s that we have a valid bootinfo
				 * structure that was completely filled in. */
				freebsd_howto |= 0x80000000;
			}

			(*entry)(freebsd_howto, NODEV, 0, 0, 0, &info.bsdinfo, 0, 0, 0);
			longjmp(restart_etherboot, -2);
		}
#endif
/*
 *	If IMAGE_MULTIBOOT is not defined, we use a boot protocol for
 *	ELF images with a couple of Etherboot extensions, namely the
 *	use of a flag bit to indicate when the image will return to
 *	Etherboot, and passing certain arguments to the image.
 */
#ifdef	IMAGE_MULTIBOOT
		cleanup();
		/* Etherboot limits the command line to the kernel name,
		 * default parameters and user prompted parameters.  All of
		 * them are shorter than 256 bytes.  As the kernel name and
		 * the default parameters come from the same BOOTP/DHCP entry
		 * (or if they don't, the parameters are empty), only two
		 * strings of the maximum size are possible.  Note this buffer
		 * can overrun if a stupid file name is chosen.  Oh well.  */
		c = cmdline;
		for (i = 0; KERNEL_BUF[i] != 0; i++) {
			switch (KERNEL_BUF[i]) {
			case ' ':
			case '\\':
			case '"':
				*c++ = '\\';
				break;
			default:
			}
			*c++ = KERNEL_BUF[i];
		}
#ifdef	IMAGE_MENU
		/* Add the default command line parameters (from the BOOTP/DHCP
		 * vendor tags specifying the available boot images) first.
		 * This is half done in bootmenu.c, because it would take a
		 * considerable amount of code to locate the selected entry and
		 * go through the colon-separated list of items.  Most of this
		 * is already done there, so this is the cheap way.  */
		if (defparams) {
			*c++ = ' ';
			for (i = 0; (i < defparams_max) &&
			            (defparams[i] != ':'); i++) {
				if (defparams[i] == '~') {
					i++;
					if (i >= defparams_max)
						continue;
					switch (defparams[i]) {
					case 'c':
						*c++ = ':';
						break;
					case 'b':
						*c++ = '\\';
						break;
					default:
						*c++ = defparams[i];
					}
				} else {
					*c++ = defparams[i];
				}
			}
		}
		/* RFC1533_VENDOR_ADDPARM is always the first option added to
		 * the BOOTP/DHCP option list.  */
		if (end_of_rfc1533 &&
		    (end_of_rfc1533[0] == RFC1533_VENDOR_ADDPARM)) {
			*c++ = ' ';
			memcpy(c, end_of_rfc1533 + 2, end_of_rfc1533[1]);
			c += end_of_rfc1533[1];
		}
#endif
		(void)sprintf(c, " -retaddr %#x", (unsigned long)xend);

		info.mbinfo.flags = 5;
		info.mbinfo.memlower = basememsize();
		info.mbinfo.memupper = memsize();
		info.mbinfo.bootdev = 0;	/* not booted from disk */
		info.mbinfo.cmdline = cmdline;

		/* The Multiboot 0.6 spec requires all segment registers to be
		 * loaded with an unrestricted, writeable segment.  All but two
		 * are already loaded, just do the rest here.  */
		__asm__ __volatile__(
			"pushl %%ds\n\t"
			"pushl %%ds\n\t"
			"popl %%fs\n\t"
			"popl %%gs"
			: /* no outputs */
			: /* no inputs */);

		/* Start the kernel, passing the Multiboot information record
		 * and the magic number.  */
		__asm__ __volatile__(
			"call *%2"
			: /* no outputs */
			: "a" (0x2BADB002), "b" (&info.mbinfo), "g" (entry)
			: "ecx","edx","esi","edi","cc","memory");
		longjmp(restart_etherboot, -2);
#else	/* !IMAGE_MULTIBOOT, i.e. generic ELF */
		/* Call cleanup only if program will not return */
		if ((info.elf32.e_flags & ELF_PROGRAM_RETURNS_BIT) == 0)
			cleanup();
		{	/* new scope so we can have local variables */
			int result, (*entry)(struct ebinfo *, union infoblock *, struct bootpd_t *);
			entry = (int (*)())info.elf32.e_entry;
			result = (*entry)(&loaderinfo, &info, BOOTP_DATA_ADDR);
			printf("Secondary program returned %d\n", result);
			if ((info.elf32.e_flags & ELF_PROGRAM_RETURNS_BIT) == 0) {
				/* We shouldn't have returned */
				result = -2;
			}
			longjmp(restart_etherboot, result);
		}
#endif	/* IMAGE_MULTIBOOT */
	}

	offset = 0;
	do {
		if (segment != -1) {
			if (skip) {
				if (skip >= len - offset) {
					skip -= len - offset;
					break;
				}
				offset += skip;
				skip = 0;
			}

			if (toread) {
				if (toread >= len - offset) {
					memcpy((void *)curaddr, data+offset,
					       len - offset);
					curaddr += len - offset;
					toread -= len - offset;
					break;
				}
				memcpy((void *)curaddr, data+offset, toread);
				offset += toread;
#ifdef	IMAGE_FREEBSD
				/* Count the bytes read even for the last block
				 * as we will need to know where the last block
				 * ends in order to load the symbols correctly.
				 * (plus it could be useful elsewhere...)
				 * Note that we need to count the actual size,
				 * not just the end of the disk image size.
				 */
				curaddr += toread;
				if (segment) curaddr += (phdr[segment].p_memsz - phdr[segment].p_filesz);
#endif
				toread = 0;
			}
		}

		/* Data left, but current segment finished - look for the next
		 * segment (in file offset order) that needs to be loaded.  The
		 * entries in the program header table are usually sorted by
		 * file offset, but someone should check the ELF specs to make
		 * sure this is actually part of the spec.  We cannot seek, so
		 * read the data in the correct order.  If the debug info is to
		 * be loaded (as per Multiboot 0.6 spec), there must be a PHDR
		 * for it that loads it to a suitable address.  There is no
		 * other simple solution, as the section headers may be
		 * somewhere in the middle of the executable file.  */
		segment = 0;
		for (i = 1; i < info.elf32.e_phnum; i++) {
			if (phdr[i].p_type != PT_LOAD)
				continue;
			if (phdr[i].p_offset < loc + offset)
				continue;	/* can't go backwards */
			if ((phdr[segment].p_type == PT_LOAD) &&
			    (phdr[i].p_offset >= phdr[segment].p_offset))
				continue;	/* search minimum file offset */
			segment = i;
		}
		if (phdr[segment].p_type != PT_LOAD) {
#ifdef	IMAGE_FREEBSD
			/* No more segments to be loaded - time to start the
			 * nasty state machine to support the loading of
			 * FreeBSD debug symbols due to the fact that FreeBSD
			 * uses/exports the kernel's debug symbols in order
			 * to make much of the system work!  Amazing (arg!)
			 *
			 * We depend on the fact that for the FreeBSD kernel,
			 * there is only one section of debug symbols and that
			 * the section is after all of the loaded sections in
			 * the file.  This assumes a lot but is somewhat required
			 * to make this code not be too annoying.  (Where do you
			 * load symbols when the code has not loaded yet?)
			 * Since this function is actually just a callback from
			 * the network data transfer code, we need to be able to
			 * work with the data as it comes in.  There is no chance
			 * for doing a seek other than forwards.
			 *
			 * The process we use is to first load the section
			 * headers.  Once they are loaded (shdr != 0) we then
			 * look for where the symbol table and symbol table
			 * strings are and setup some state that we found
			 * them and fall into processing the first one (which
			 * is the symbol table) and after that has been loaded,
			 * we try the symbol strings.  Note that the order is
			 * actually required as the memory image depends on
			 * the symbol strings being loaded starting at the
			 * end of the symbol table.  The kernel assumes this
			 * layout of the image.
			 *
			 * At any point, if we get to the end of the load file
			 * or the section requested is earlier in the file than
			 * the current file pointer, we just end up falling
			 * out of this and booting the kernel without this
			 * information.
			 */

			/* Make sure that the next address is long aligned... */
			/* Assumes size of long is a power of 2... */
			curaddr = (curaddr + sizeof(long) - 1) & ~(sizeof(long) - 1);

			/* If we have not yet gotten the shdr loaded, try that */
			if (shdr == 0)
			{
				toread = info.elf32.e_shnum * info.elf32.e_shentsize;
				skip = info.elf32.e_shoff - (loc + offset);
				if (toread)
				{
#ifdef	DEBUG_ELF
					printf("shdr *, size %X, curaddr %X\n", toread, curaddr);
#endif

					/* Start reading at the curaddr and make that the shdr */
					shdr = (Elf32_Shdr *)curaddr;

					/* Start to read... */
					continue;
				}
			}
			else
			{
				/* We have the shdr loaded, check if we have found
				 * the indexs where the symbols are supposed to be */
				if ((symtabindex == -1) && (symstrindex == -1))
				{
					/* Make sure that the address is page aligned... */
					/* Symbols need to start in their own page(s)... */
					curaddr = (curaddr + 4095) & ~4095;

					/* Need to make new indexes... */
					for (i=0; i < info.elf32.e_shnum; i++)
					{
						if (shdr[i].sh_type == SHT_SYMTAB)
						{
							int j;
							for (j=0; j < info.elf32.e_phnum; j++)
							{
								/* Check only for loaded sections */
								if ((phdr[i].p_type | 0x80) == (PT_LOAD | 0x80))
								{
									/* Only the extra symbols */
									if ((shdr[i].sh_offset >= phdr[j].p_offset) &&
									    ((shdr[i].sh_offset + shdr[i].sh_size) <=
									     (phdr[j].p_offset + phdr[j].p_filesz)))
									{
										shdr[i].sh_offset=0;
										shdr[i].sh_size=0;
										break;
									}
								}
							}
							if ((shdr[i].sh_offset != 0) && (shdr[i].sh_size != 0))
							{
								symtabindex = i;
								symstrindex = shdr[i].sh_link;
							}
						}
					}
				}

				/* Check if we have a symbol table index and have not loaded it */
                                if ((symtab_load == 0) && (symtabindex >= 0))
				{
					/* No symbol table yet?  Load it first... */

					/* This happens to work out in a strange way.
					 * If we are past the point in the file already,
					 * we will skip a *large* number of bytes which
					 * ends up bringing us to the end of the file and
					 * an old (default) boot.  Less code and lets
					 * the state machine work in a cleaner way but this
					 * is a nasty side-effect trick... */
					skip = shdr[symtabindex].sh_offset - (loc + offset);

					/* And we need to read this many bytes... */
					toread = shdr[symtabindex].sh_size;

					if (toread)
					{
#ifdef	DEBUG_ELF
						printf("db sym, size %X, curaddr %X\n", toread, curaddr);
#endif
						/* Save where we are loading this... */
						symtab_load = curaddr;

						*((long *)curaddr) = toread;
						curaddr += sizeof(long);

						/* Start to read... */
						continue;
					}
				}
				else if ((symstr_load == 0) && (symstrindex >= 0))
				{
					/* We have already loaded the symbol table, so
					 * now on to the symbol strings... */


					/* Same nasty trick as above... */
					skip = shdr[symstrindex].sh_offset - (loc + offset);

					/* And we need to read this many bytes... */
					toread = shdr[symstrindex].sh_size;

					if (toread)
					{
#ifdef	DEBUG_ELF
						printf("db str, size %X, curaddr %X\n", toread, curaddr);
#endif
						/* Save where we are loading this... */
						symstr_load = curaddr;

						*((long *)curaddr) = toread;
						curaddr += sizeof(long);

						/* Start to read... */
						continue;
					}
				}
			}
#endif	/* IMAGE_FREEBSD */

			/* No more segments to be loaded, so just start the
			 * kernel.  This saves a lot of network bandwidth if
			 * debug info is in the kernel but not loaded.  */
			goto elf_startkernel;
			break;
		}
		/* May have to fix before calling program */
		phdr[segment].p_type |= 0x80;	/* ignore next time */
		curaddr = phdr[segment].p_paddr;
		skip = phdr[segment].p_offset - (loc + offset);
		toread = phdr[segment].p_filesz;
#ifdef	DEBUG_ELF
		printf("PHDR %d, size %#X, curaddr %#X\n",
		       segment, toread, curaddr);
#endif
	} while (offset < len);

	loc += len;
	return 1;
}
#endif

int os_download(unsigned int block, unsigned char *data, unsigned int len)
{
	if (block == 1)
	{
#if	defined(AOUT_IMAGE) || defined(ELF_IMAGE)
		memcpy(&info, data, sizeof(info));
#endif
#ifdef	AOUT_IMAGE
		if (info.s[0] == 0x010B) {
			printf("(a.out");
			image_type = Aout;
#ifdef	IMAGE_FREEBSD
			if (info.s[1] == 0) {
				/* Some other a.out variants have a different
				 * value, and use other alignments (e.g. 1K),
				 * not the 4K used by FreeBSD.  */
				image_type = Aout_FreeBSD;
				printf("/FreeBSD");
				off = -(info.head.a_entry & 0xff000000);
				info.head.a_entry += off;
			}
#endif
			printf(")... ");
			segment = -1;
			loc = 0;
			skip = 0;
			toread = 0;
		} else
#endif
#ifdef	ELF_IMAGE
		if (info.l[0] == 0x464C457Fl) {
			printf("(ELF");
			image_type = Elf;
#ifdef	IMAGE_FREEBSD
			if (info.elf32.e_entry & 0xf0000000) {
				image_type = Elf_FreeBSD;
				printf("/FreeBSD");
				off = -(info.elf32.e_entry & 0xff000000);
				info.elf32.e_entry += off;
			}
#endif
			printf(")... ");
			if ((info.elf32.e_type != ET_EXEC) ||
			    ((info.elf32.e_machine != EM_386) &&
			     (info.elf32.e_machine != EM_486)) ||
			    (info.elf32.e_version != 1)) {
				printf("invalid ELF file for machine type\n");
				return 0;
			}
			if (info.elf32.e_phoff +
			    info.elf32.e_phnum * info.elf32.e_phentsize > len) {
				printf("ELF header outside first block\n");
				return 0;
			}
			phdr=(Elf32_Phdr *)((unsigned int)&info
					+ (unsigned int)(info.elf32.e_phoff));
			/* Check for Etherboot related limitations.  Memory
			 * below 0x10000 and between RELOC and 0xfffff is not
			 * allowed.  Reasons: the RTL8139 hack, the Etherboot
			 * code/data area and the ROM/IO area.  */
			for (segment = 0; segment < info.elf32.e_phnum;
			     segment++) {
				unsigned int e;
				if (phdr[segment].p_type != PT_LOAD)
					continue;
				if (phdr[segment].p_paddr < 0x10000) {
					printf("segment below 0x10000\n");
					return 0;
				}
#ifdef	IMAGE_FREEBSD
				if (image_type == Elf_FreeBSD) {
					phdr[segment].p_paddr += off;
				}
#endif
				e = phdr[segment].p_paddr+phdr[segment].p_memsz;
				if ((phdr[segment].p_paddr < 0x100000) &&
				    (e > RELOC)) {
					printf("segment in reserved area\n");
					return 0;
				}
				if (e > memsize() * 1024 + 0x100000) {
					printf("segment exceeding memory\n");
					return 0;
				}
			}
			segment = -1;
			loc = 0;
			skip = 0;
			toread = 0;
#ifdef	IMAGE_FREEBSD
			/* Make sure we have a null to start with... */
			shdr = 0;

			/* Clear the symbol index values... */
			symtabindex = -1;
			symstrindex = -1;

			/* ...and the load addresses of the symbols  */
			symtab_load = 0;
			symstr_load = 0;
#endif
		} else
#endif
#ifdef	TAGGED_IMAGE
		{
			printf("(NBI)... ");
			image_type = Tagged;
			/* Zero all context info */
			memset(&tctx, 0, sizeof(tctx));
			/* Copy first 4 longwords */
			memcpy(&tctx.img, data, sizeof(tctx.img));
			/* Memory location where we are supposed to save it */
			tctx.segaddr = tctx.linlocation = ((tctx.img.u.segoff.ds) << 4) + tctx.img.u.segoff.bx;
			/* Grab a copy */
			memcpy((void *)tctx.segaddr, data, 512);
			/* Advance to first segment descriptor */
			tctx.segaddr += ((tctx.img.length & 0x0F) << 2)
				+ ((tctx.img.length & 0xF0) >> 2);
			if (len > 512) {
				len -= 512;
				data += 512;
				/* and fall through to deal with rest of
				   block */
			} else
				return (1);
		}
#endif	/* TAGGED_IMAGE */
		/* Empty clause in case TAGGED_IMAGE is not defined
		   but ELF_IMAGE is */
		{
		}
#ifdef	SIZE_INDICATOR
		printf("XXXX");
#endif
	} /* end of block zero processing */
	switch (image_type) {
#ifdef	TAGGED_IMAGE
	case Tagged:
		return (tagged_download(data, len));
		break;
#endif
#ifdef	AOUT_IMAGE
	case Aout:
#ifdef	IMAGE_FREEBSD
	case Aout_FreeBSD:
#endif
		return (aout_download(data, len));
		break;
#endif
#ifdef	ELF_IMAGE
	case Elf:
#ifdef	IMAGE_FREEBSD
	case Elf_FreeBSD:
#endif
		return (elf_download(data, len));
		break;
#endif
	default:
		break;
	}
	return (1);
}

/*
 * Local variables:
 *  c-basic-offset: 8
 * End:
 */

