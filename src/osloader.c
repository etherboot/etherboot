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

struct os_entry_regs os_regs;

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
#include "elf.h"

struct multiboot_mods {
	unsigned mod_start;
	unsigned mod_end;
	unsigned char *string;
	unsigned reserved;
};

/* The structure of a Multiboot 0.6 parameter block.  */
struct multiboot_info {
	unsigned int flags;
#define MULTIBOOT_MEM_VALID       0x01
#define MULTIBOOT_BOOT_DEV_VALID  0x02
#define MULTIBOOT_CMDLINE_VALID   0x04
#define MULTIBOOT_MODS_VALID      0x08
#define MULTIBOOT_AOUT_SYMS_VALID 0x10
#define MULTIBOOT_ELF_SYMS_VALID  0x20
#define MULTIBOOT_MMAP_VALID      0x40
	unsigned int memlower;
	unsigned int memupper;
	unsigned int bootdev;
	unsigned int cmdline;	/* physical address of the command line */
	unsigned mods_count;
	struct multiboot_mods *mods_addr;
	unsigned syms_num;
	unsigned syms_size;
	unsigned syms_addr;
	unsigned syms_shndx;
	unsigned mmap_length;
	struct e820entry *mmap_addr;
	/* The structure actually ends here, so I might as well put
	 * the ugly e820 parameters here...
	 */
	unsigned e820entry_size;
	struct e820entry mmap[E820MAX];
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
	Elf64_Ehdr elf64;
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

#ifdef WINCE_IMAGE
static int ce_loader(unsigned char *data, unsigned int len);
static int get_x_header(unsigned char *data, unsigned long now);
static void jump_2ep();
static unsigned char ce_signature[] = {'B', '0', '0', '0', 'F', 'F', '\n',};
static char ** ep;

#define BOOT_ARG_PTR_LOCATION 0x001FFFFC

typedef struct _BOOT_ARGS{
	unsigned char ucVideoMode;
	unsigned char ucComPort;
	unsigned char ucBaudDivisor;
	unsigned char ucPCIConfigType;
	
	unsigned long dwSig;
	#define BOOTARG_SIG 0x544F4F42
	unsigned long dwLen;
	
	unsigned char ucLoaderFlags;
	unsigned char ucEshellFlags;
	unsigned char ucEdbgAdapterType;
	unsigned char ucEdbgIRQ;
	
	unsigned long dwEdbgBaseAddr;
	unsigned long dwEdbgDebugZone;	
	unsigned long dwDHCPLeaseTime;
	unsigned long dwEdbgFlags;
	
	unsigned long dwEBootFlag;
	unsigned long dwEBootAddr;
	unsigned long dwLaunchAddr;
	
	unsigned long pvFlatFrameBuffer;
	unsigned short vesaMode;
	unsigned short cxDisplayScreen;
	unsigned short cyDisplayScreen;
	unsigned short cxPhysicalScreen;
	unsigned short cyPhysicalScreen;
	unsigned short cbScanLineLength;
	unsigned short bppScreen;
	
	unsigned char RedMaskSize;
	unsigned char REdMaskPosition;
	unsigned char GreenMaskSize;
	unsigned char GreenMaskPosition;
	unsigned char BlueMaskSize;
	unsigned char BlueMaskPosition;
}BOOT_ARGS;

BOOT_ARGS BootArgs;

static struct segment_info{
	unsigned long addr;		// Section Address
	unsigned long size;		// Section Size
	unsigned long checksum;		// Section CheckSum
}X;

#define DSIZE  512+12
static unsigned long dbuffer_available =0;
static unsigned long not_loadin =0;
static unsigned long d_now =0;

unsigned long entry;
static unsigned long ce_curaddr;



#endif /* WINCE_IMAGE */


#if defined(IMAGE_FREEBSD)
static enum {
	Unknown, Tagged, Aout, Elf, Aout_FreeBSD, Elf_FreeBSD,
} image_type = Unknown;
#endif

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
	unsigned char		first;
} tctx;

#define	TAGGED_PROGRAM_RETURNS	(tctx.img.length & 0x00000100)	/* bit 8 */
#define	LINEAR_EXEC_ADDR	(tctx.img.length & 0x80000000)	/* bit 31 */

#endif

#if	defined(AOUT_IMAGE) || defined(ELF_IMAGE) || defined(ELF64_IMAGE)
static union infoblock	info;
static int segment;		/* current segment number, -1 for none */
static unsigned long loc;	/* start offset of current block */
static unsigned long skip;	/* padding to be skipped to current segment */
static unsigned long toread;	/* remaining data to be read in the segment */
#endif

#if defined (ELF_IMAGE) || defined (ELF64_IMAGE)
#define ELF_NOTES 1
#define ELF_DEBUG 0
#endif

#if ELF_NOTES
int check_ip_checksum;
uint16_t ip_checksum;
unsigned long ip_checksum_offset;
#endif

#ifdef ELF64_IMAGE
static Elf64_Phdr *phdr64;
static uint64_t loc64;
static uint64_t skip64;
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



static void done(void)
{
#ifdef	SIZEINDICATOR
	printf("K ");
#endif
	printf("done\n");
#ifdef	DELIMITERLINES
	{
		int j;
		for (j=0; j<80; j++)
			putchar('=');
		putchar('\n');
	}
#endif
	cleanup();
}

#if defined(ELF_IMAGE) || defined (ELF64_IMAGE) || defined(AOUT_IMAGE) || defined(TAGGED_IMAGE)
static int prep_segment(unsigned long start, unsigned long mid, unsigned long end,
	unsigned long istart, unsigned long iend)
{
	int fit, i;
	if (mid > end) {
		printf("filesz > memsz\n");
		return 0;
	}
	if ((end > virt_to_phys(_text)) && 
		(start < virt_to_phys(_end))) {
		printf("segment [%X, %X) overlaps etherboot [%X, %X)\n",
			start, end,
			virt_to_phys(_text), virt_to_phys(_end)
			);
		return 0;
	}
	if ((end > heap_ptr) && (start < heap_bot)) {
		printf("segment [%X, %X) overlaps heap [%X, %X)\n",
			start, end,
			heap_ptr, heap_bot
			);
		return 0;
	}
	fit = 0;
	for(i = 0; i < meminfo.map_count; i++) {
		unsigned long long r_start, r_end;
		if (meminfo.map[i].type != E820_RAM)
			continue;
		r_start = meminfo.map[i].addr;
		r_end = r_start + meminfo.map[i].size;
		if ((start >= r_start) && (end <= r_end)) {
			fit = 1;
			break;
		}
	}
	if (!fit) {
		printf("\nsegment [%X,%X) does not fit in any memory region\n",
			start, end);
#if ELF_DEBUG
		printf("Memory regions(%d):\n", meminfo.map_count);
		for(i = 0; i < meminfo.map_count; i++) {
			unsigned long long r_start, r_end;
			if (meminfo.map[i].type != E820_RAM)
				continue;
			r_start = meminfo.map[i].addr;
			r_end = r_start + meminfo.map[i].size;
			printf("[%X%X, %X%X) type %d\n", 
				(unsigned long)(r_start >> 32),
				(unsigned long)r_start,
				(unsigned long)(r_end >> 32),
				(unsigned long)r_end,
				meminfo.map[i].type);
		}
#endif
		return 0;
	}
	/* Zero the bss */
	if (end > mid) {
		memset(phys_to_virt(mid), 0, end - mid);
	}
#if ELF_NOTES
	if (check_ip_checksum) {
		if ((istart <= ip_checksum_offset) && 
			(iend > ip_checksum_offset)) {
			/* The checksum note is also loaded in a
			 * PT_LOAD segment, so the computed checksum
			 * should be 0.
			 */
			ip_checksum = 0;
		}
	}
#endif
	return 1;
}
#endif

#if defined(ELF_IMAGE) || defined (ELF64_IMAGE)
static void boot(unsigned long entry)
{
	/*
	 *	If IMAGE_MULTIBOOT is not defined, we use a boot protocol for
	 *	ELF images with a couple of Etherboot extensions, namely the
	 *	use of a flag bit to indicate when the image will return to
	 *	Etherboot, and passing certain arguments to the image.
	 */
#ifdef	IMAGE_MULTIBOOT
	unsigned char cmdline[512], *c;
	int i;
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
	(void)sprintf(c, " -retaddr %#X", virt_to_phys(xend32));

	info.mbinfo.flags = MULTIBOOT_MMAP_VALID | MULTIBOOT_MEM_VALID |MULTIBOOT_CMDLINE_VALID;
	info.mbinfo.memlower = meminfo.basememsize;
	info.mbinfo.memupper = meminfo.memsize;
	info.mbinfo.bootdev = 0;	/* not booted from disk */
	info.mbinfo.cmdline = virt_to_phys(cmdline);
	info.mbinfo.e820entry_size = sizeof(struct e820entry);
	info.mbinfo.mmap_length = 
		info.mbinfo.e820entry_size * meminfo.map_count;
	info.mbinfo.mmap_addr = info.mbinfo.mmap;
	memcpy(info.mbinfo.mmap, meminfo.map, info.mbinfo.mmap_length);
	
	/* The Multiboot 0.6 spec requires all segment registers to be
	 * loaded with an unrestricted, writeable segment.
	 * xstart32 does this for us.
	 */
	
	/* Start the kernel, passing the Multiboot information record
	 * and the magic number.  */
	os_regs.eax = 0x2BADB002;
	os_regs.ebx = virt_to_phys(&info.mbinfo);
	xstart32(entry);
	longjmp(restart_etherboot, -2);
#else	/* !IMAGE_MULTIBOOT, i.e. generic ELF */
	int result;
	/* We cleanup unconditionally, and then reawaken the network
	 * adapter after the longjmp.
	 */
	result = xstart32(entry,
		virt_to_phys(&loaderinfo),
		virt_to_phys(&info),
		virt_to_phys(BOOTP_DATA_ADDR));
	printf("Secondary program returned %d\n", result);
	longjmp(restart_etherboot, result);
#endif	/* IMAGE_MULTIBOOT */

}
#endif

#ifdef	TAGGED_IMAGE
static sector_t tagged_download(unsigned char *data, unsigned int len, int eof);
static inline os_download_t tagged_probe(unsigned char *data, unsigned int len)
{
	struct segheader	*sh;
	unsigned long loc;
	if (*((uint32_t *)data) != 0x1B031336L) {
		return 0;
	}
	printf("(NBI)");
	/* If we don't have enough data give up */
	if (len < 512)
		return 0;
	/* Zero all context info */
	memset(&tctx, 0, sizeof(tctx));
	/* Copy first 4 longwords */
	memcpy(&tctx.img, data, sizeof(tctx.img));
	/* Memory location where we are supposed to save it */
	tctx.segaddr = tctx.linlocation = 
		((tctx.img.u.segoff.ds) << 4) + tctx.img.u.segoff.bx;
	if (!prep_segment(tctx.segaddr, tctx.segaddr + 512, tctx.segaddr + 512,
			  0, 512)) {
		return 0;
	}
	/* Now verify the segments we are about to load */
	loc = 512;
	for(sh = (struct segheader *)(data
				      + ((tctx.img.length & 0x0F) << 2)
				      + ((tctx.img.length & 0xF0) >> 2) ); 
		(sh->length > 0) && ((unsigned char *)sh < data + 512); 
		sh = (struct segheader *)((unsigned char *)sh
					  + ((sh->length & 0x0f) << 2) + ((sh->length & 0xf0) >> 2)) ) {
		if (!prep_segment(
			sh->loadaddr,
			sh->loadaddr + sh->imglength,
			sh->loadaddr + sh->memlength,
			loc, loc + sh->imglength)) {
			return 0;
		}
		loc = loc + sh->imglength;
		if (sh->flags & 0x04) 
			break;
	}
	if (!(sh->flags & 0x04))
		return 0;
	/* Grab a copy */
	memcpy(phys_to_virt(tctx.segaddr), data, 512);
	/* Advance to first segment descriptor */
	tctx.segaddr += ((tctx.img.length & 0x0F) << 2)
		+ ((tctx.img.length & 0xF0) >> 2);
	/* Remember to skip the first 512 data bytes */
	tctx.first = 1;
	
	return tagged_download;

}
static sector_t tagged_download(unsigned char *data, unsigned int len, int eof)
{
	int	i;

	if (tctx.first) {
		tctx.first = 0;
		if (len > 512) {
			len -= 512;
			data += 512;
			/* and fall through to deal with rest of block */
		} else 
			return 0;
	}
	do {
		if (len == 0) /* Detect truncated files */
			eof = 0;
		while (tctx.seglen == 0) {
			struct segheader	sh;
			if (tctx.segflags & 0x04) {
				done();
				if (LINEAR_EXEC_ADDR) {
					int result;
					/* no gateA20_unset for PM call */
					result = xstart32(tctx.img.execaddr,
						virt_to_phys(&loaderinfo),
						tctx.linlocation,
						virt_to_phys(BOOTP_DATA_ADDR));
					printf("Secondary program returned %d\n",
						result);
					if (!TAGGED_PROGRAM_RETURNS) {
						/* We shouldn't have returned */
						result = -2;
					}
					longjmp(restart_etherboot, result);
						
				} else {
					gateA20_unset();
					xstart16(tctx.img.execaddr, 
						tctx.img.u.location, 
						virt_to_phys(BOOTP_DATA_ADDR));
					longjmp(restart_etherboot, -2);
				}
			}
			sh = *((struct segheader *)phys_to_virt(tctx.segaddr));
			tctx.seglen = sh.imglength;
			if ((tctx.segflags = sh.flags & 0x03) == 0)
				curaddr = sh.loadaddr;
			else if (tctx.segflags == 0x01)
				curaddr = tctx.last1 + sh.loadaddr;
			else if (tctx.segflags == 0x02)
				curaddr = (Address)(meminfo.memsize * 1024L
						    + 0x100000L)
					- sh.loadaddr;
			else
				curaddr = tctx.last0 - sh.loadaddr;
			tctx.last1 = (tctx.last0 = curaddr) + sh.memlength;
			tctx.segflags = sh.flags;
			tctx.segaddr += ((sh.length & 0x0F) << 2)
				+ ((sh.length & 0xF0) >> 2);
			/* Avoid lock-up */
			if ( sh.length == 0 ) longjmp(restart_etherboot, -2); 
		}
		i = (tctx.seglen > len) ? len : tctx.seglen;
		memcpy(phys_to_virt(curaddr), data, i);
		tctx.seglen -= i;
		curaddr += i;
		len -= i;
		data += i;
	} while ((len > 0) || eof);
	return 0;
}
#endif

#ifdef	AOUT_IMAGE
static sector_t aout_download(unsigned char *data, unsigned int len, int eof);
static inline os_download_t aout_probe(unsigned char *data, unsigned int len)
{
	unsigned long start, mid, end, istart, iend;
	if (info.s[0] != 0x010BL) {
		return 0;
	}
	printf("(a.out");
#ifdef	IMAGE_FREEBSD
	image_type = Aout;
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
	/* Check the aout image */
	start  = info.head.a_entry;
	mid    = (((start + info.head.a_text) + 4095) & ~4095) + info.head.a_data;
	end    = ((mid + 4095) & ~4095) + info.head.a_bss;
	istart = 4096;
	iend   = istart + (mid - start);
	if (!prep_segment(start, mid, end, istart, iend))
		return 0;
	segment = -1;
	loc = 0;
	skip = 0;
	toread = 0;
	return aout_download;
}

static sector_t aout_download(unsigned char *data, unsigned int len, int eof)
{
	unsigned int offset;	/* working offset in the current data block */

	offset = 0;

#ifdef AOUT_LYNX_KDI
	segment++;
	if (segment == 0) {
		curaddr = 0x100000;
		info.head.a_entry = curaddr + 0x20;
	}
	memcpy(phys_to_virt(curaddr), data, len);
	curaddr += len;
	return 0;
#endif

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
					memcpy(phys_to_virt(curaddr), data+offset,
						len - offset);
					curaddr += len - offset;
					toread -= len - offset;
					break;
				}
				memcpy(phys_to_virt(curaddr), data+offset, toread);
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
			memset(phys_to_virt(curaddr), '\0', info.head.a_bss);
			goto aout_startkernel;
		default:
			break;
		}
	} while (offset < len);

	loc += len;

	if (eof) {
		int	j;
		unsigned long entry;

aout_startkernel:
		entry = info.head.a_entry;
		done();

#ifdef	IMAGE_FREEBSD
		if (image_type == Aout_FreeBSD) {
			memset(&info.bsdinfo, 0, sizeof(info.bsdinfo));
			info.bsdinfo.bi_basemem = meminfo.basememsize;
			info.bsdinfo.bi_extmem = meminfo.memsize;
			info.bsdinfo.bi_memsizes_valid = 1;
			info.bsdinfo.bi_version = BOOTINFO_VERSION;
			info.bsdinfo.bi_kernelname = virt_to_phys(KERNEL_BUF);
			info.bsdinfo.bi_nfs_diskless = NULL;
			info.bsdinfo.bi_size = sizeof(info.bsdinfo);
			xstart32(entry, freebsd_howto, NODEV, 0, 0, 0, 
				virt_to_phys(&info.bsdinfo), 0, 0, 0);
			longjmp(restart_etherboot, -2);
		}
#endif
#ifdef AOUT_LYNX_KDI
		xstart32(entry);
#endif
		printf("unexpected a.out variant\n");
		longjmp(restart_etherboot, -2);
	}
	return 0;
}
#endif

#ifdef ELF_NOTES
unsigned long add_ip_checksums(unsigned long offset, unsigned long sum, unsigned long new)
{
	unsigned long checksum;
	sum = ~sum & 0xFFFF;
	new = ~new & 0xFFFF;
	if (offset & 1) {
		/* byte swap the sum if it came from an odd offset 
		 * since the computation is endian independant this
		 * works.
		 */
		new = ((new >> 8) & 0xff) | ((new << 8) & 0xff00);
	}
	checksum = sum + new;
	if (checksum > 0xFFFF) {
		checksum -= 0xFFFF;
	}
	return (~checksum) & 0xFFFF;
}

static void process_elf_notes(unsigned char *header,
	unsigned long offset, unsigned long length)
{
	unsigned char *note, *end;
	char *program, *version;

	check_ip_checksum = 0;
	note = header + offset;
	end = note + length;
	program = version = 0;
	while(note < end) {
		Elf_Nhdr *hdr;
		unsigned char *n_name, *n_desc, *next;
		hdr = (Elf_Nhdr *)note;
		n_name = note + sizeof(*hdr);
		n_desc = n_name + ((hdr->n_namesz + 3) & ~3);
		next = n_desc + ((hdr->n_descsz + 3) & ~3);
		if (next > end) {
			break;
		}
		if ((hdr->n_namesz == sizeof(ELF_NOTE_BOOT)) && 
			(memcmp(n_name, ELF_NOTE_BOOT, sizeof(ELF_NOTE_BOOT)) == 0)) {
			switch(hdr->n_type) {
			case EIN_PROGRAM_NAME:
				if (n_desc[hdr->n_descsz -1] == 0) {
					program = n_desc;
				}
				break;
			case EIN_PROGRAM_VERSION:
				if (n_desc[hdr->n_descsz -1] == 0) {
					version = n_desc;
				}
				break;
			case EIN_PROGRAM_CHECKSUM:
				check_ip_checksum = 1;
				ip_checksum = *((uint16_t *)n_desc);
				/* Remember where the segment is so
				 * I can detect segment overlaps.
				 */
				ip_checksum_offset = n_desc - header;
				break;
			}
		}
#if ELF_DEBUG
		printf("n_type: %x n_name(%d): %s n_desc(%d): %s\n", 
			hdr->n_type,
			hdr->n_namesz, n_name,
			hdr->n_descsz, n_desc);
#endif
		note = next;
	}
	if (program && version) {
		printf("Loading %s version: %s\n",
			program, version);
	}
}
#endif

#ifdef  ELF64_IMAGE
static sector_t elf64_download(unsigned char *data, unsigned int len, int eof);
static inline os_download_t elf64_probe(unsigned char *data, unsigned int len)
{
	if ((info.elf64.e_ident[EI_MAG0] != ELFMAG0) ||
		(info.elf64.e_ident[EI_MAG1] != ELFMAG1) ||
		(info.elf64.e_ident[EI_MAG2] != ELFMAG2) ||
		(info.elf64.e_ident[EI_MAG3] != ELFMAG3) ||
		(info.elf64.e_ident[EI_CLASS] != ELFCLASS64) ||
		(info.elf64.e_ident[EI_DATA] != ELFDATA_CURRENT) ||
		(info.elf64.e_ident[EI_VERSION] != EV_CURRENT) ||
		(info.elf64.e_type != ET_EXEC) ||
		(info.elf64.e_machine != EM_CURRENT) ||
		(info.elf64.e_version != EV_CURRENT) ||
		(info.elf64.e_ehsize != sizeof(Elf64_Ehdr)) ||
		(info.elf64.e_phentsize != sizeof(Elf64_Phdr))) {
		return 0;
	}
	printf("(ELF64)... ");
	if (info.elf64.e_phoff +
		info.elf64.e_phnum * info.elf64.e_phentsize > len) {
		printf("ELF header outside first block\n");
		return 0;
	}
	if (info.elf64.e_entry > ULONG_MAX) {
		printf("ELF entry point exceeds address space\n");
		return 0;
	}
	phdr64 = (Elf64_Phdr *)(info.dummy + info.elf64.e_phoff);
#if ELF_NOTES
	/* Load ELF notes from the image */
	for(segment = 0; segment < info.elf32.e_phnum; segment++) {
		if (phdr[segment].p_type != PT_NOTE)
			continue;
		if (phdr[segment].p_offset + phdr[segment].p_filesz > len) {
			/* Ignore ELF notes outside of the first block */
			continue;
		}
		process_elf_notes(data, 
			phdr[segment].p_offset, phdr[segment].p_filesz);
	}
#endif
	/* Check for Etherboot related limitations.  Memory
	 * between _text and _end is not allowed.  
	 * Reasons: the Etherboot code/data area.
	 */
	for (segment = 0; segment < info.elf64.e_phnum; segment++) {
		unsigned long start, mid, end, istart, iend;
		if (phdr64[segment].p_type != PT_LOAD) 
			continue;
		if ((phdr64[segment].p_paddr > ULONG_MAX) ||
			((phdr64[segment].p_paddr + phdr64[segment].p_filesz) > ULONG_MAX) ||
			((phdr64[segment].p_paddr + phdr64[segment].p_memsz) > ULONG_MAX)) {
			printf("ELF segment exceeds address space\n");
			return 0;
		}
		start = phdr64[segment].p_paddr;
		mid = start + phdr64[segment].p_filesz;
		end = start + phdr64[segment].p_memsz;
		istart = iend = ULONG_MAX;
		if ((phdr64[segment].p_offset < ULONG_MAX) &&
			((phdr64[segment].p_offset + phdr64[segment].p_filesz) < ULONG_MAX))
		{
			istart = phdr64[segment].p_offset;
			iend   = istart + phdr64[segment].p_filesz;
		} 
		if (!prep_segment(start, mid, end, istart, iend)) {
			return 0;
		}
	}
	segment = -1;
	loc64 = 0;
	skip64 = 0;
	toread = 0;
	return elf64_download;
}

static sector_t elf64_download(unsigned char *data, unsigned int len, int eof)
{
	unsigned long skip_sectors = 0;
	unsigned int offset;	/* working offset in the current data block */
	int i;

	offset = 0;
	do {
		if (segment != -1) {
			if (skip64) {
				if (skip64 >= len - offset) {
					skip64 -= len - offset;
					break;
				}
				offset += skip64;
				skip64 = 0;
			}
			
			if (toread) {
				unsigned int cplen;
				cplen = len - offset;
				if (cplen >= toread) {
					cplen = toread;
				}
				memcpy(phys_to_virt(curaddr), data+offset, cplen);
				curaddr += cplen;
				toread -= cplen;
				offset += cplen;
				if (toread)
					break;
			}
		}
		
		/* Data left, but current segment finished - look for the next
		 * segment (in file offset order) that needs to be loaded. 
		 * We can only seek forward, so select the program headers,
		 * in the correct order.
		 */
		segment = -1;
		for (i = 0; i < info.elf64.e_phnum; i++) {
			if (phdr64[i].p_type != PT_LOAD)
				continue;
			if (phdr64[i].p_filesz == 0)
				continue;
			if (phdr64[i].p_offset < loc64 + offset)
				continue;	/* can't go backwards */
			if ((segment != -1) &&
				(phdr64[i].p_offset >= phdr64[segment].p_offset))
				continue;	/* search minimum file offset */
			segment = i;
		}
		if (segment == -1) {
			/* No more segments to be loaded, so just start the
			 * kernel.  This saves a lot of network bandwidth if
			 * debug info is in the kernel but not loaded.  */
			goto elf_startkernel;
			break;
		}
		curaddr = phdr64[segment].p_paddr;
		skip64 = phdr64[segment].p_offset - (loc64 + offset);
		toread = phdr64[segment].p_filesz;
#if ELF_DEBUG
		printf("PHDR %d, size %#X, curaddr %#X\n",
			segment, toread, curaddr);
#endif
	} while (offset < len);
	
	loc64 += len + (skip64 & ~0x1ff);
	skip_sectors = skip64 >> 9;
	skip64 &= 0x1ff;
	
	if (eof) {
elf_startkernel:
		done();
		boot(info.elf64.e_entry);
	}
	return skip_sectors;
}

#endif /* ELF64_IMAGE */
#ifdef	ELF_IMAGE
static sector_t elf_download(unsigned char *data, unsigned int len, int eof);
static inline os_download_t elf_probe(unsigned char *data, unsigned int len)
{
	if ((info.elf32.e_ident[EI_MAG0] != ELFMAG0) ||
		(info.elf32.e_ident[EI_MAG1] != ELFMAG1) ||
		(info.elf32.e_ident[EI_MAG2] != ELFMAG2) ||
		(info.elf32.e_ident[EI_MAG3] != ELFMAG3) ||
		(info.elf32.e_ident[EI_CLASS] != ELFCLASS32) ||
		(info.elf32.e_ident[EI_DATA] != ELFDATA_CURRENT) ||
		(info.elf32.e_ident[EI_VERSION] != EV_CURRENT) ||
		(info.elf32.e_type != ET_EXEC) ||
		(info.elf32.e_machine != EM_CURRENT) ||
		(info.elf32.e_version != EV_CURRENT) ||
		(info.elf32.e_ehsize != sizeof(Elf32_Ehdr)) ||
		(info.elf32.e_phentsize != sizeof(Elf32_Phdr))) {
		return 0;
	}
	printf("(ELF");
#ifdef	IMAGE_FREEBSD
	image_type = Elf;
	if (info.elf32.e_entry & 0xf0000000) {
		image_type = Elf_FreeBSD;
		printf("/FreeBSD");
		off = -(info.elf32.e_entry & 0xff000000);
		info.elf32.e_entry += off;
	}
#endif
	printf(")... ");
	if (info.elf32.e_phoff +
		info.elf32.e_phnum * info.elf32.e_phentsize > len) {
		printf("ELF header outside first block\n");
		return 0;
	}
	phdr = (Elf32_Phdr *)(info.dummy + info.elf32.e_phoff);
#if ELF_NOTES
	/* Load ELF notes from the image */
	for(segment = 0; segment < info.elf32.e_phnum; segment++) {
		if (phdr[segment].p_type != PT_NOTE)
			continue;
		if (phdr[segment].p_offset + phdr[segment].p_filesz > len) {
			/* Ignore ELF notes outside of the first block */
			continue;
		}
		process_elf_notes(data, 
			phdr[segment].p_offset, phdr[segment].p_filesz);
	}
#endif
	/* Check for Etherboot related limitations.  Memory
	 * between _text and _end is not allowed.
	 * Reasons: the Etherboot code/data area.
	 */
	for (segment = 0; segment < info.elf32.e_phnum; segment++) {
		unsigned long start, mid, end, istart, iend;
		if (phdr[segment].p_type != PT_LOAD)
			continue;
#ifdef	IMAGE_FREEBSD
		if (image_type == Elf_FreeBSD) {
			phdr[segment].p_paddr += off;
		}
#endif
		start = phdr[segment].p_paddr;
		mid = start + phdr[segment].p_filesz;
		end = start + phdr[segment].p_memsz;
		istart = phdr[segment].p_offset;
		iend = istart + phdr[segment].p_filesz;
		if (!prep_segment(start, mid, end, istart, iend)) {
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
	return elf_download;
}

static sector_t elf_download(unsigned char *data, unsigned int len, int eof)
{
	unsigned long skip_sectors = 0;
	unsigned int offset;	/* working offset in the current data block */
	int i;

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
				unsigned int cplen;
				cplen = len - offset;
				if (cplen >= toread) {
					cplen = toread;
				}
				memcpy(phys_to_virt(curaddr), data+offset, cplen);
				curaddr += cplen;
				toread -= cplen;
				offset += cplen;
				if (toread)
					break;
#ifdef IMAGE_FREEBSD
				/* Count the bytes read even for the last block
				 * as we will need to know where the last block
				 * ends in order to load the symbols correctly.
				 * (plus it could be useful elsewhere...)
				 * Note that we need to count the actual size,
				 * not just the end of the disk image size.
				 */
				if (segment) {
					curaddr += phdr[segment].p_memsz - phdr[segment].p_filesz;
				}
#endif
			}
		}
		
		/* Data left, but current segment finished - look for the next
		 * segment (in file offset order) that needs to be loaded. 
		 * We can only seek forward, so select the program headers,
		 * in the correct order.
		 */
		segment = -1;
		for (i = 0; i < info.elf32.e_phnum; i++) {
			if (phdr[i].p_type != PT_LOAD)
				continue;
			if (phdr[i].p_filesz == 0)
				continue;
			if (phdr[i].p_offset < loc + offset)
				continue;	/* can't go backwards */
			if ((segment != -1) &&
				(phdr[i].p_offset >= phdr[segment].p_offset))
				continue;	/* search minimum file offset */
			segment = i;
		}
		if (segment == -1) {
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
#if ELF_DEBUG
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
#if ELF_DEBUG
						printf("db sym, size %X, curaddr %X\n", toread, curaddr);
#endif
						/* Save where we are loading this... */
						symtab_load = curaddr;
						
						*((long *)phys_to_virt(curaddr)) = toread;
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
#if ELF_DEBUG
						printf("db str, size %X, curaddr %X\n", toread, curaddr);
#endif
						/* Save where we are loading this... */
						symstr_load = curaddr;
						
						*((long *)phys_to_virt(curaddr)) = toread;
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
		curaddr = phdr[segment].p_paddr;
		skip    = phdr[segment].p_offset - (loc + offset);
		toread  = phdr[segment].p_filesz;
#if ELF_DEBUG
		printf("PHDR %d, size %#X, curaddr %#X\n",
			segment, toread, curaddr);
#endif
	} while (offset < len);

	loc += len + (skip & ~0x1ff);
	skip_sectors = skip >> 9;
	skip &= 0x1ff;
	
	if (eof) {
		unsigned long entry;
elf_startkernel:
		done();
		entry = info.elf32.e_entry;
#ifdef IMAGE_FREEBSD
		if (image_type == Elf_FreeBSD) {
			memset(&info.bsdinfo, 0, sizeof(info.bsdinfo));
			info.bsdinfo.bi_basemem = meminfo.basememsize;
			info.bsdinfo.bi_extmem = meminfo.memsize;
			info.bsdinfo.bi_memsizes_valid = 1;
			info.bsdinfo.bi_version = BOOTINFO_VERSION;
			info.bsdinfo.bi_kernelname = virt_to_phys(KERNEL_BUF);
			info.bsdinfo.bi_nfs_diskless = NULL;
			info.bsdinfo.bi_size = sizeof(info.bsdinfo);
#define RB_BOOTINFO     0x80000000      /* have `struct bootinfo *' arg */  
			if(freebsd_kernel_env[0] != '\0'){
				freebsd_howto |= RB_BOOTINFO;
				info.bsdinfo.bi_envp = (unsigned long)freebsd_kernel_env;
			}

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
				t = phys_to_virt(info.bsdinfo.bi_esymtab);

#if ELF_DEBUG
				printf("Metadata at %X\n",t);
#endif

				/* Set up the pointer to the memory... */
				info.bsdinfo.bi_modulep = virt_to_phys(t);
				
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
				info.bsdinfo.bi_kernend = virt_to_phys(t);

				/* Signal locore.s that we have a valid bootinfo
				 * structure that was completely filled in. */
				freebsd_howto |= 0x80000000;
			}

			xstart32(entry, freebsd_hofwto, NODEV, 0, 0, 0, 
				virt_to_phys(&info.bsdinfo), 0, 0, 0);
			longjmp(restart_etherboot, -2);
		}
#endif
#if ELF_NOTES
		if (check_ip_checksum) {
			unsigned long bytes = 0;
			uint16_t sum, new_sum;

			sum = ipchksum(&info.elf32, sizeof(info.elf32));
			bytes = sizeof(info.elf32);
#if ELF_DEBUG
			printf("Ehdr: %hx sz: %x bytes: %x\n",
				sum, bytes, bytes);
#endif

			new_sum = ipchksum(phdr, sizeof(phdr[0]) * info.elf32.e_phnum);
			sum = add_ip_checksums(bytes, sum, new_sum);
			bytes += sizeof(phdr[0]) * info.elf32.e_phnum;
#if ELF_DEBUG
			printf("Phdr: %hx sz: %x bytes: %x\n",
				new_sum, 
				sizeof(phdr[0]) * info.elf32.e_phnum, bytes);
#endif

			for(i = 0; i < info.elf32.e_phnum; i++) {
				if (phdr[i].p_type != PT_LOAD)
					continue;
				new_sum = ipchksum(phys_to_virt(phdr[i].p_paddr),
						phdr[i].p_memsz);
				sum = add_ip_checksums(bytes, sum, new_sum);
				bytes += phdr[i].p_memsz;
#if ELF_DEBUG
			printf("seg%d: %hx sz: %x bytes: %x\n",
				i, new_sum, 
				phdr[i].p_memsz, bytes);
#endif

			}
			if (ip_checksum != sum) {
				printf("Image checksum: %hx != computed checksum: %hx\n",
					ip_checksum, sum);
				longjmp(restart_etherboot, -2);
			}
		}
#endif
		boot(entry);
	}
	return skip_sectors;
}
#endif

#ifdef WINCE_IMAGE
static sector_t ce_loader(unsigned char *data, unsigned int len, int eof);
static os_download_t wince_probe(unsigned char *data, unsigned int len)
{
	if (strncmp(ce_signature, data, sizeof(ce_signature)) != 0) {
		return 0;
	}
	printf("(WINCE)");
	return ce_loader;
}
static sector_t ce_loader(unsigned char *data, unsigned int len, int eof)
{
	unsigned char dbuffer[DSIZE];
	int this_write = 0;
	static int firsttime = 1;

	/* FIXME handle non 512 byte packets! */

	/*
	 *	new packet in, we have to 
	 *	[1] copy data to dbuffer,
	 *
	 *	update...
	 *	[2]  dbuffer_available
	 */
	memcpy( (dbuffer+dbuffer_available), data, 512);	//[1]
	dbuffer_available += 512;	// [2]
	len = 0;

	d_now = 0;
	
#if 0
	printf("dbuffer_available =%d \n", dbuffer_available);
#endif 
	
	if (firsttime) 
	{
		d_now = sizeof(ce_signature);
		printf("String Physical Address = %x \n", 
			*(unsigned long *)(dbuffer+d_now));
		
		d_now += sizeof(unsigned long);
		printf("Image Size = %d [%x]\n", 
			*(unsigned long *)(dbuffer+d_now), 
			*(unsigned long *)(dbuffer+d_now));
		
		d_now += sizeof(unsigned long);
		dbuffer_available -= d_now;			
		
		d_now = (unsigned long)get_x_header(dbuffer, d_now);
		firsttime = 0;
	}
	
	if (not_loadin == 0)
	{
		d_now = get_x_header(dbuffer, d_now);
	}
	
	while ( not_loadin > 0 )
	{
		/* dbuffer do not have enough data to loading, copy all */
#if 0
		printf("[0] not_loadin = [%d], dbuffer_available = [%d] \n", 
			not_loadin, dbuffer_available);
		printf("[0] d_now = [%d] \n", d_now);
#endif
		
		if( dbuffer_available <= not_loadin)
		{
			this_write = dbuffer_available ;
			memcpy(phys_to_virt(ce_curaddr), (dbuffer+d_now), this_write );
			ce_curaddr += this_write;
			not_loadin -= this_write;
			
			/* reset index and available in the dbuffer */
			dbuffer_available = 0;
			d_now = 0;
#if 0
			printf("[1] not_loadin = [%d], dbuffer_available = [%d] \n", 
				not_loadin, dbuffer_available);
			printf("[1] d_now = [%d], this_write = [%d] \n", 
				d_now, this_write);
#endif
				
			// get the next packet...
			return (0);
		}
			
		/* dbuffer have more data then loading ... , copy partital.... */
		else
		{
			this_write = not_loadin;
			memcpy(phys_to_virt(ce_curaddr), (dbuffer+d_now), this_write);
			ce_curaddr += this_write;
			not_loadin = 0;
			
			/* reset index and available in the dbuffer */
			dbuffer_available -= this_write;
			d_now += this_write;
#if 0
			printf("[2] not_loadin = [%d], dbuffer_available = [%d] \n", 
				not_loadin, dbuffer_available);
			printf("[2] d_now = [%d], this_write = [%d] \n\n", 
				d_now, this_write);
#endif
			
			/* dbuffer not empty, proceed processing... */
			
			// don't have enough data to get_x_header..
			if ( dbuffer_available < (sizeof(unsigned long) * 3) )
			{
//				printf("we don't have enough data remaining to call get_x. \n");
				memcpy( (dbuffer+0), (dbuffer+d_now), dbuffer_available);
				return (0);
			}
			else
			{
#if 0				
				printf("with remaining data to call get_x \n");
				printf("dbuffer available = %d , d_now = %d\n", 
					dbuffer_available, d_now);
#endif					
				d_now = get_x_header(dbuffer, d_now);
			}
		}
	}
	return (0);
}

static int get_x_header(unsigned char *dbuffer, unsigned long now)
{
	X.addr = *(unsigned long *)(dbuffer + now);
	X.size = *(unsigned long *)(dbuffer + now + sizeof(unsigned long));
	X.checksum = *(unsigned long *)(dbuffer + now + sizeof(unsigned long)*2);

	ce_curaddr = X.addr;
	now += sizeof(unsigned long)*3;

	/* re-calculate dbuffer available... */
	dbuffer_available -= sizeof(unsigned long)*3;

	/* reset index of this section */
	not_loadin = X.size;
	
#if 1
	printf("\n");
	printf("\t Section Address = [%x] \n", X.addr);
	printf("\t Size = %d [%x]\n", X.size, X.size);
	printf("\t Checksum = %d [%x]\n", X.checksum, X.checksum);
#endif
#if 0
	printf("____________________________________________\n");
	printf("\t dbuffer_now = %d \n", now);
	printf("\t dbuffer available = %d \n", dbuffer_available);
	printf("\t not_loadin = %d \n", not_loadin);
#endif

	if(X.addr == 0)
	{
		entry = X.size;
		done();
		printf("Entry Point Address = [%x] \n", entry);
		jump_2ep();		
	}
	
	return now;
}

static void jump_2ep()
{
	BootArgs.ucVideoMode = 1;
	BootArgs.ucComPort = 1;
	BootArgs.ucBaudDivisor = 1;
	BootArgs.ucPCIConfigType = 1;	// do not fill with 0
	
	BootArgs.dwSig = BOOTARG_SIG;
	BootArgs.dwLen = sizeof(BootArgs);
	
	if(BootArgs.ucVideoMode == 0)
	{
		BootArgs.cxDisplayScreen = 640;
		BootArgs.cyDisplayScreen = 480;
		BootArgs.cxPhysicalScreen = 640;
		BootArgs.cyPhysicalScreen = 480;
		BootArgs.bppScreen = 16;
		BootArgs.cbScanLineLength  = 1024;
		BootArgs.pvFlatFrameBuffer = 0x800a0000;	// ollie say 0x98000000
	}	
	else if(BootArgs.ucVideoMode != 0xFF)
	{
		BootArgs.cxDisplayScreen = 0;
		BootArgs.cyDisplayScreen = 0;
		BootArgs.cxPhysicalScreen = 0;
		BootArgs.cyPhysicalScreen = 0;
		BootArgs.bppScreen = 0;
		BootArgs.cbScanLineLength  = 0;
		BootArgs.pvFlatFrameBuffer = 0;	
	}

	ep = phys_to_virt(BOOT_ARG_PTR_LOCATION);
	*ep= virt_to_phys(BootArgs);
	xstart32(entry);
}
#endif /* WINCE_IMAGE */

#if defined(X86_BOOTSECTOR_IMAGE) && defined(PCBIOS)
int bios_disk_dev = 0;
#define BOOTSECT (0x7C00)
static unsigned long x86_bootsector_download(
	unsigned char *data, unsigned int len, int eof);
static os_download_t x86_bootsector_probe(unsigned char *data, unsigned int len)
{
	if (*((uint16_t *)(data + 510)) != 0xAA55) {
		return 0;
	}
	printf("(X86)");
	return x86_bootsector_download;
}
static unsigned long x86_bootsector_download(unsigned char *data, unsigned int len,int eof)
{
	if (len != 512) {
		printf("Wrong size bootsector\n");
		return 0;
	}
	memcpy(phys_to_virt(BOOTSECT), data, len);
	done();
	gateA20_unset();
	/* Set %edx to device number to emulate BIOS
	   Fortunately %edx is not used after this */
	__asm__("movl %0,%%edx" : : "g" (bios_disk_dev));
	xstart16(BOOTSECT, 0, 0);
	printf("Bootsector returned?");
	longjmp(restart_etherboot, -2);
	return 0;
}
#endif


/**************************************************************************
PROBE_IMAGE - Detect image file type
**************************************************************************/
os_download_t probe_image(unsigned char *data, unsigned int len)
{
	os_download_t os_download = 0;
#if defined(ELF_IMAGE) || defined(AOUT_IMAGE) || defined(ELF64_IMAGE)
	memcpy(&info, data, sizeof(info));
#endif
#ifdef AOUT_IMAGE
	if (!os_download) os_download = aout_probe(data, len);
#endif
#ifdef ELF_IMAGE
	if (!os_download) os_download = elf_probe(data, len);
#endif
#ifdef ELF64_IMAGE
	if (!os_download) os_download = elf64_probe(data, len);
#endif
#ifdef WINCE_IMAGE
	if (!os_download) os_download = wince_probe(data, len);
#endif
#ifdef TAGGED_IMAGE
	if (!os_download) os_download = tagged_probe(data, len);
#endif
	/* FIXME merge the PXE loader && the X86_BOOTSECTOR_IMAGE
	 * loader.  They may need a little BIOS context information
	 * telling them how they booted, but otherwise they
	 * are essentially the same code.
	 */
#ifdef FREEBSD_PXEEMU
	if (!os_download) os_download = pxe_probe(data, len);
#endif
#ifdef X86_BOOTSECTOR_IMAGE
	if (!os_download) os_download = x86_bootsector_probe(data, len);
#endif
	return os_download;
}

/**************************************************************************
LOAD_BLOCK - Try to load file
**************************************************************************/
int load_block(unsigned char *data, unsigned int block, unsigned int len, int eof)
{
	static os_download_t os_download;
	static sector_t skip_sectors;
	static unsigned int skip_bytes;
#ifdef	SIZEINDICATOR
	static int rlen = 0;

	if (block == 1)
	{
		rlen=len;
		printf("XXXX");
	}
	if (!(block % 4) || eof) {
		int size;
		size = ((block-1) * rlen + len) / 1024;

		putchar('\b');
		putchar('\b');
		putchar('\b');
		putchar('\b');

		putchar('0' + (size/1000)%10);
		putchar('0' + (size/100)%10);
		putchar('0' + (size/10)%10);
		putchar('0' + (size/1)%10);
	}
#endif
	if (block == 1)
	{
		skip_sectors = 0;
		skip_bytes = 0;
		os_download = probe_image(data, len);
		if (!os_download) {
			printf("error: not a valid image\n");
#if 0
			printf("block: %d len: %d\n", block, len);
			printf("%hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx\n",
				data[0], data[1], data[2], data[3],
				data[4], data[5], data[6], data[7]);
#endif
			return 0;
		}
	} /* end of block zero processing */

	/* Either len is greater or the skip is greater */
	if ((skip_sectors > (len >> 9)) ||
		((skip_sectors == (len >> 9)) && (skip_bytes >= (len & 0x1ff)))) {
		if (skip_bytes > len) {
			skip_bytes -= len;
		}
		else {
			skip_sectors -= (len - skip_bytes + 511) >> 9;
			skip_bytes = 512 - ((len - skip_bytes) & 0x1ff);
		}
	}
	else {
		len -= (skip_sectors << 9) + skip_bytes;
		data += (skip_sectors << 9) + skip_bytes;
		skip_sectors = os_download(data, len, eof);
		skip_bytes = 0;
	}
	return 1;
}

/*
 * Local variables:
 *  c-basic-offset: 8
 * End:
 */

