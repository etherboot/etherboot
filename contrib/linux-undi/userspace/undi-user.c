#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/sysmacros.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/major.h>
#include <errno.h>

// Etherboot headers

#define STDINT_H
typedef unsigned           size_t;

typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned long      uint32_t;
typedef unsigned long long uint64_t;

typedef signed char        int8_t;
typedef signed short       int16_t;
//typedef signed long        int32_t;
typedef signed long long   int64_t;

typedef signed char	   s8;
typedef unsigned char      u8;

typedef signed short       s16;
typedef unsigned short     u16;

typedef signed int         s32;
typedef unsigned int       u32;

typedef signed long long   s64;
typedef unsigned long long u64;

#define TFTP_MAX_PACKET         1432 /* 512 */

#define PAGE_SIZE (4096)
#include <if_ether.h>
#include <pxe.h>
#include <callbacks_arch.h>

#undef inb
#undef inb_p
#undef inw
#undef inw_p
#undef inl
#undef inl_p


#undef outb
#undef outb_p
#undef outw
#undef outw_p
#undef outl
#undef outl_p

#include <sys/io.h>
#include <asm/ldt.h>
#include <linux/unistd.h>
#include "undi.h"

int modify_ldt(int func, void *ptr, unsigned long bytecount);

pxe_t *g_pxe;

unsigned long int virt_offset = 0;

#define DEV_FNAME "/dev/undimem"

static void hexdump(char *loc, int len);

static pxe_t *hunt_pixie ( void ) ;

void *entrypoint32;

void call_pxe(int opcode, void *arg) {
	int ignored0, ignored1, ignored2, ignored3, ignored4, ignored5;
	asm (
	     "pushw %%bx\n" // seg
	     "pushw %%bx\n" // offset
	     "pushw %%ax\n" // opcode
	     "call *%%ecx\n"
	     : "=a" (ignored0), "=b" (ignored1), "=c" (ignored2), "=d" (ignored3),
	       "=D" (ignored4), "=S" (ignored5)
	     :  "a" (opcode), "b" (arg), "c" (entrypoint32) );
}

int *log_position;
char *log_base;
char caller_log_buf[1024];

void print_log(void) {
	printf(caller_log_buf);
}

int orig_esp;

int main(int argc, char **argv) {
	struct stat file_stat;
	orig_esp = &file_stat;
	if(stat(DEV_FNAME, &file_stat) != 0) {
		if(errno == ENOENT) {
			mode_t  mode = S_IFCHR;
			if(mknod(DEV_FNAME, mode, 
				 makedev(MISC_MAJOR, UNDI_MEM_MINOR)) != 0) {
				printf("mknod(%s): error %d!\n", DEV_FNAME, errno);
				exit(-1);
			}
		} else {
			printf("stat(%s): Unknown error!\n", DEV_FNAME);
			exit(-1);
		}
	}

	if(chmod(DEV_FNAME, S_IRUSR | S_IWUSR) != 0) {
		printf("chmod(%s): error\n", DEV_FNAME);
		exit(-1);
	}
	int undi_mem_fd = open(DEV_FNAME, O_RDWR);
	if(undi_mem_fd < 0) {
		printf("Could not open %s\n", DEV_FNAME);
		exit(-1);
	}

	printf("About to mmap()\n");
	if(ioctl(undi_mem_fd, UNDI_MEM_MAP_PXE_SEARCH) != 0) {
		printf("ioctl failed\n");
		exit(-1);
	}
	void *base = mmap(PXE_SEARCH_MAP_BASE, PXE_SEARCH_MAP_LENGTH,
			  PROT_READ | PROT_WRITE | PROT_EXEC,
			  MAP_FIXED | MAP_PRIVATE,
			  undi_mem_fd, 0);
	// printf("mmap fd = %d rv = %d errno = %d\n", undi_mem_fd, rv, errno);
	if(base == MAP_FAILED) {
		printf("mmap() error %d!\n", errno);
		return -1;
	}
	printf("mmap() => %p\n", base);

#if 0
	printf("Dumping\n");
	int dumpfile = open(DUMP_FNAME, O_WRONLY);
	if(dumpfile < 0) {
		printf("error opening dumpfile\n");
	} else {
		write(dumpfile, );
	}
#endif

	printf("About to hunt\n");
	g_pxe = hunt_pixie();

	if(g_pxe == NULL) {
		printf("Could not find pxe!\n");
		return -1;
	}
	log_position = (int*)((char*)(g_pxe) + sizeof(*g_pxe));
	*(char **)(log_position + 1) = caller_log_buf;
	log_base = (char *) (log_position + 2);
	printf(" initial data = %s\n", log_base + 1);

	entrypoint32 = (g_pxe->EntryPointESP.segment << 16) | 
		g_pxe->EntryPointESP.offset;

	uint32_t pxe_base = g_pxe->BC_Data.Phy_Addr;
	uint32_t pxe_length = (g_pxe->BC_Data.Seg_Addr << 16) | 
		g_pxe->BC_Data.Seg_Size;

	printf("PXE segment is %p:%d, entry point is %p\n", pxe_base, pxe_length, entrypoint32);

	printf("Using undimem device to map in pxe segment pages\n");
	uint32_t pxe_base_page_aligned = pxe_base & ~(PAGE_SIZE - 1),
		pxe_length_page_aligned = (((pxe_base + pxe_length) - pxe_base_page_aligned) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
	struct UNDI_Exec_Ctl exec_ctl = {
		.mem_base = pxe_base_page_aligned,
		.mem_length = pxe_length_page_aligned,
	};
	if(ioctl(undi_mem_fd, UNDI_MEM_MAP_UNDI_EXEC, &exec_ctl) != 0) {
		printf("ioctl error %d\n", errno);
		exit(-1);
	}
	void *exec_base = mmap(pxe_base_page_aligned, pxe_length_page_aligned,
			  PROT_READ | PROT_WRITE | PROT_EXEC,
			  MAP_FIXED | MAP_PRIVATE,
			  undi_mem_fd, 0);
	if(exec_base == MAP_FAILED) {
		printf("mmap execution region failed\n");
		exit(-1);
	}
	printf("exec_base = %p\n", exec_base);

	int num_descs = g_pxe->SegDescCn;
	printf("Modifying LDT, need %d entries\n", num_descs);

	// sanity check alignment
#if 0
typedef struct {
        uint16_t                Seg_Addr;
        uint32_t                Phy_Addr;
        uint16_t                Seg_Size;
} PACKED I386_SEGDESC_t;  /* PACKED is required, otherwise gcc pads
                          * this out to 12 bytes -
                          * mbrown@fensystems.co.uk (mcb30) 17/5/03 */
#endif

	SEGDESC_t *descs = &g_pxe->Stack;
	if(descs + 1 != &g_pxe->UNDIData) {
		printf("UNDI segment alignment is weird!\n");
		return -1;
	}

	int i;

	SEGSEL_t first_selector;
	printf("Updating PXE with first selector %x\n", first_selector);
	/*
	  API Hack:
		[3] = BC_Data -- CS
		[4] = BC_Code -- SS,DS,ES
	*/

	// Index = 3, LDT, CPL3
	first_selector = (0 << 3) | (0x4 /* LDT */ | 0x3 /* RPL 3 */);
	g_pxe->FirstSelector = first_selector;

	// modify ldt
	/*
typedef struct modify_ldt_ldt_s {
        unsigned int  entry_number;
        unsigned long base_addr;
        unsigned int  limit;
        unsigned int  seg_32bit:1;
        unsigned int  contents:2; // << 10
        unsigned int  read_exec_only:1; // << 9, inverted
        unsigned int  limit_in_pages:1;
        unsigned int  seg_not_present:1;
        unsigned int  useable:1;
} modify_ldt_t;
	 */

	// b *(0x7eeedb0 + 0x1160)
	struct modify_ldt_ldt_s mod_entry_cs = {
		.entry_number = 3,
		.base_addr = pxe_base,
		.limit = pxe_length_page_aligned / PAGE_SIZE,
		.seg_32bit = 1,
		.contents = 0x2, // Code, non-conforming
		.read_exec_only = 0, // Execute/Read
		.limit_in_pages = 1,
		.seg_not_present = 0,
		.useable = 1,
	};
	struct modify_ldt_ldt_s mod_entry_ds = mod_entry_cs;
	mod_entry_ds.entry_number = 4;
	mod_entry_ds.contents = 0; // Normal data
	mod_entry_ds.read_exec_only = 0; // read/write

	// use "new mode" modify_ldt() interface (based on 2.6.16 kernel's sys_modify_ldt)
	if(modify_ldt(0x11, &mod_entry_cs, sizeof(mod_entry_cs)) == -1) {
		printf("Modify ldt(cs) error %d\n", errno); // EINVAL
	}
	if(modify_ldt(1, &mod_entry_ds, sizeof(mod_entry_ds)) == -1) {
		printf("Modify ldt(ds) error %d\n", errno);
	}
#if 0
	g_pxe->FirstSelector = first_selector;
	for(i=0; i < num_descs; i++) {
		printf("(%x,%x,%x)\n",
		       descs[i].Seg_Addr, descs[i].Phy_Addr, 
		       descs[i].Seg_Size);
		printf("Set the selector (Seg_Addr) here\n");
	}
#endif

	// Setting IOPL3
	printf("Setting IOPL3\n");
	if(iopl(3) != 0) {
		printf("IOPL error %d\n", errno);
		return -1;
	}
	printf("PXE test call (press any key)\n");
	char buf[100];
	fgets(buf, sizeof(buf), stdin);
	call_pxe(0xdead, 0xbeef);
	print_log();
	return 0;
}

// Utility and PXE scan functions based on Etherboot code

/**************************************************************************
 * Utility functions
 **************************************************************************/

/* Checksum a block.
 */

static uint8_t checksum ( void *block, size_t size ) {
	uint8_t sum = 0;
	uint16_t i = 0;
	for ( i = 0; i < size; i++ ) {
		sum += ( ( uint8_t * ) block )[i];
	}
	return sum;
}

/* Print the status of a !PXE structure
 */

static void pxe_dump ( pxe_t *pxe ) {
	printf ( "API %hx:%hx ( %hx:%hx ) St %hx:%hx UD %hx:%hx UC %hx:%hx "
		 "BD %hx:%hx BC %hx:%hx\n",
		 pxe->EntryPointSP.segment, pxe->EntryPointSP.offset,
		 pxe->EntryPointESP.segment, pxe->EntryPointESP.offset,
		 pxe->Stack.Seg_Addr, pxe->Stack.Seg_Size,
		 pxe->UNDIData.Seg_Addr, pxe->UNDIData.Seg_Size,
		 pxe->UNDICode.Seg_Addr, pxe->UNDICode.Seg_Size,
		 pxe->BC_Data.Seg_Addr, pxe->BC_Data.Seg_Size,
		 pxe->BC_Code.Seg_Addr, pxe->BC_Code.Seg_Size );
}


static pxe_t *hunt_pixie ( void ) {
	static uint32_t ptr = 0;
	pxe_t *pxe = NULL;

	printf ( "Hunting for pixies..." );
	if ( ptr == 0 ) ptr = 0xa0000;
	while ( ptr > 0x10000 ) {
		ptr -= 16;
		pxe = (pxe_t *) phys_to_virt ( ptr );
		if ( memcmp ( pxe->Signature, "!PXE", 4 ) == 0 ) {
			printf ( "found !PXE at %08x...", (int)ptr );
			if ( checksum ( pxe, sizeof(pxe_t) ) != 0 ) {
				printf ( "invalid checksum\n..." );
				continue;
			}
			printf ( "ok, really found at %p\n", (void *)ptr );
			pxe_dump(pxe);
#if 0
			undi.pxe = pxe;
			pxe_dump();
			printf ( "Resetting pixie...\n" );
			undi_unload_base_code();
			eb_pxenv_stop_undi();
			pxe_dump();
#endif
			return pxe;
		} else {
			int i;
#if 0
			printf("%c%c%c%c\n", 
			       ((char *)ptr)[0],
			       ((char *)ptr)[1],
			       ((char *)ptr)[2],
			       ((char *)ptr)[3]);
#endif
		}
	}
	printf ( "none found\n" );
	ptr = 0;
	return NULL;
}

static void hexdump(char *loc, int len) {
	int i;
	for(i=0; i < len; i++) {
		printf("%02x ", (int)(unsigned char)loc[i]);
		if(i > 0 && (i + 1) %16 == 0) {
			printf("\n");
		}
	}
	if(i % 16 != 0) {
		printf("\n");
	}
}

void peek(char *location, int len) {
	hexdump(location, len);
}
