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

**************************************************************************/

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 */

#include "etherboot.h"

#ifdef	ETHERBOOT32
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

/* The structure of a Multiboot 0.6 parameter block.  */
struct multiboot_info {
	unsigned int flags;
	unsigned int memlower;
	unsigned int memupper;
	unsigned int bootdev;
	void *cmdline;
};
#endif	/* ETHERBOOT32 */

/* Define some structures used by tftp loader */

struct imgheader
{
	unsigned long magic;
	unsigned char length;
	unsigned char r[3];
	struct { unsigned short bx, ds; } location;
	struct { unsigned short ip, cs; } execaddr;
};

union infoblock
{
	unsigned short s[256];
	unsigned long l[128];
	struct imgheader img;
#ifdef	ETHERBOOT32
	struct exec head;
	struct bootinfo bsdinfo;
	struct multiboot_info mbinfo;
	Elf32_Ehdr elf32;
#endif
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
static unsigned char	segflags;
static unsigned long	seglen;
static Address		last0, last1, execaddr, hdraddr, segaddr;
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
#endif

#ifdef  IMAGE_FREEBSD
static unsigned int off;
#endif

#ifdef	TAGGED_IMAGE
static int tagged_download(unsigned char *data, unsigned int len)
{
	int	i,j;

	do {
		while (seglen == 0) {
			struct segheader	sh;
			if (segflags & 0x04) {

#ifdef	SIZEINDICATOR
				printf("K ");
#endif
				cleanup_net();
				printf("done\n");
#ifdef	DELIMITERLINES
				for (j=0; j<80; j++) putchar('=');
				putchar('\n');
#endif
				cleanup();
				gateA20_unset();

#ifdef	ETHERBOOT32
				xstart(execaddr, hdraddr, (char *)BOOTP_DATA_ADDR);
#endif
#ifdef	ETHERBOOT16
				xstart(execaddr, hdraddr, BOOTP_DATA_ADDR, RELOC>>4);
#endif
				longjmp(jmp_bootmenu, 2);
			}
#ifdef	ETHERBOOT32
			sh = *((struct segheader *)segaddr);
#endif
#ifdef	ETHERBOOT16
			fmemcpy(&sh, segaddr, sizeof(struct segheader));
#endif
			seglen = sh.imglength;
			if ((segflags = sh.flags & 0x03) == 0)
				curaddr = sh.loadaddr;
			else if (segflags == 0x01)
				curaddr = last1 + sh.loadaddr;
			else if (segflags == 0x02)
				curaddr = (Address)(memsize() * 1024L
						    + 0x100000L)
					- sh.loadaddr;
			else
				curaddr = last0 - sh.loadaddr;
			last1 = (last0 = curaddr) + sh.memlength;
			segflags = sh.flags;
			segaddr += ((sh.length & 0x0F) << 2)
				+ ((sh.length & 0xF0) >> 2);
		}
		i = (seglen > len) ? len : seglen;
#ifdef	ETHERBOOT32
		memcpy((void *)curaddr, data, i);
#endif
#ifdef	ETHERBOOT16
		memcpyf(curaddr, data, i);
#endif
		seglen -= i;
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
		cleanup_net();
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
			info.bsdinfo.bi_kernelname = kernel;
			info.bsdinfo.bi_nfs_diskless = NULL;
			info.bsdinfo.bi_size = sizeof(info.bsdinfo);
			(*entry)(freebsd_howto, NODEV, 0, 0, 0, &info.bsdinfo, 0, 0, 0);
			longjmp(jmp_bootmenu, 2);
		}
#endif
		printf("unexpected a.out variant\n");
		longjmp(jmp_bootmenu, 2);
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
		unsigned char cmdline[512], *c;

elf_startkernel:
		entry = (void *)info.elf32.e_entry;
		cleanup_net();
		printf("done\n");
#ifdef	DELIMITERLINES
		for (j=0; j<80; j++) putchar('=');
		putchar('\n');
#endif
		cleanup();

#ifdef IMAGE_FREEBSD
		if (image_type == Elf_FreeBSD) {
			memset(&info.bsdinfo, 0, sizeof(info.bsdinfo));
			info.bsdinfo.bi_basemem = basememsize();
			info.bsdinfo.bi_extmem = memsize();
			info.bsdinfo.bi_memsizes_valid = 1;
			info.bsdinfo.bi_version = BOOTINFO_VERSION;
			info.bsdinfo.bi_kernelname = kernel;
			info.bsdinfo.bi_nfs_diskless = NULL;
			info.bsdinfo.bi_size = sizeof(info.bsdinfo);
			(*entry)(freebsd_howto, NODEV, 0, 0, 0, &info.bsdinfo, 0, 0, 0);
			longjmp(jmp_bootmenu, 2);
		}
#endif
#ifdef	IMAGE_MULTIBOOT
		/* Etherboot limits the command line to the kernel name,
		 * default parameters and user prompted parameters.  All of
		 * them are shorter than 256 bytes.  As the kernel name and
		 * the default parameters come from the same BOOTP/DHCP entry
		 * (or if they don't, the parameters are empty), only two
		 * strings of the maximum size are possible.  Note this buffer
		 * can overrun if a stupid file name is chosen.  Oh well.  */
		c = cmdline;
		for (i = 0; kernel[i] != 0; i++) {
			switch (kernel[i]) {
			case ' ':
			case '\\':
			case '"':
				*c++ = '\\';
				break;
			default:
			}
			*c++ = kernel[i];
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
		sprintf(c, " -retaddr %#x", (unsigned long)xend);

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
		longjmp(jmp_bootmenu, 2);
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
			/* No more segments to be loaded, so just start the
			 * kernel.  This saves a lot of network bandwidth if
			 * debug info is in the kernel but not loaded.  */
			goto elf_startkernel;
			break;
		}
		phdr[segment].p_type |= 0x80;	/* ignore next time */
		curaddr = phdr[segment].p_paddr;
		skip = phdr[segment].p_offset - (loc + offset);
		toread = phdr[segment].p_filesz;
#ifdef	DEBUG_ELF
		printf("PHDR %d, size %X, curaddr %X\n",
		       segment, toread, curaddr);
#endif
	} while (offset < len);

	loc += len;
	return 1;
}
#endif

int os_download(unsigned int block, unsigned char *data, unsigned int len)
{
	union infoblock		*ibp;

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
		} else
#endif
#ifdef	TAGGED_IMAGE
		{
			printf("(NBI)... ");
			ibp = (union infoblock *)data;
			image_type = Tagged;
			/* the apparently unnecessary cast is to avoid a bcc
			   bug where the short is not promoted as it should be */
			segaddr = (((Address)ibp->img.location.ds) << 4) + ibp->img.location.bx;
#ifdef	ETHERBOOT32
			memcpy((void *)segaddr, data, 512);
#endif
#ifdef	ETHERBOOT16
			memcpyf(segaddr, data, 512);
#endif
			execaddr = ibp->l[3];
			hdraddr = ibp->l[2];
			last1 = last0 = segaddr + 512;
			segaddr += ((ibp->img.length & 0x0F) << 2)
				+ ((ibp->img.length & 0xF0) >> 2);
			segflags = 0;
			seglen = 0;
			if (len > 512) {
				len -= 512;
				data += 512;
				/* and fall through to deal with rest of
				   block */
			} else
				return (1);
		}
#endif	/* TAGGED_IMAGE */
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

