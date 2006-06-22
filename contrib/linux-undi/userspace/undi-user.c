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
#include <if_ether.h>
#include <pxe.h>
#include <callbacks_arch.h>

#include "undi.h"

unsigned long int virt_offset = 0;

#define DEV_FNAME "/dev/undimem"

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

int main(int argc, char **argv) {
	struct stat file_stat;
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
	ioctl(undi_mem_fd, UNDI_MEM_MAP_PXE_SEARCH);
#if 1
	void *base = mmap(PXE_SEARCH_MAP_BASE, PXE_SEARCH_MAP_LENGTH,
			  PROT_READ | PROT_WRITE | PROT_EXEC,
			  MAP_FIXED | MAP_PRIVATE,
			  undi_mem_fd, 0);
#else
	void *base = mmap(NULL, 4096,
			  /* PROT_NONE */ PROT_READ /* | PROT_WRITE  | PROT_EXEC */,
			  MAP_PRIVATE /* MAP_FIXED */ /* MAP_LOCKED */, 
			  undi_mem_fd, 0);
#endif
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
	pxe_t *pxe = hunt_pixie();

	if(pxe == NULL) {
		printf("Could not find pxe!\n");
		return -1;
	}

	SEGSEL_t first_selector = 0;
	int num_descs = pxe->SegDescCn;
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

	SEGDESC_t *descs = &pxe->Stack;
	if(descs + 1 != &pxe->UNDIData) {
		printf("UNDI segment alignment is weird!\n");
		return -1;
	}
	int i;
	printf("Updating PXE with first selector %x\n", first_selector);
	pxe->FirstSelector = first_selector;
	for(i=0; i < num_descs; i++) {
		printf("(%x,%x,%x)\n",
		       descs[i].Seg_Addr, descs[i].Phy_Addr, 
		       descs[i].Seg_Size);
		printf("Set the selector (Seg_Addr) here\n");
	}

	return 0;
}
