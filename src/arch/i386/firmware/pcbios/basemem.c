#ifdef PCBIOS

#include "etherboot.h"

/* Routines to allocate base memory in a BIOS-compatible way, by
 * updating the Free Base Memory Size counter at 40:13h.
 *
 * Michael Brown <mbrown@fensystems.co.uk> (mcb30)
 * $Id$
 */

#define fbms ( ( uint16_t * ) phys_to_virt ( 0x413 ) )
#define BASE_MEMORY_MAX ( 640 )
#define FREE_BLOCK_MAGIC ( ('!'<<0) + ('F'<<8) + ('R'<<16) + ('E'<<24) )

/* #define DEBUG_BASEMEM 1 */

typedef struct free_base_memory_block {
	uint32_t	magic;
	uint16_t	size_kb;
} free_base_memory_block_t;

/* Return amount of free base memory in bytes
 */

uint32_t get_free_base_memory ( void ) {
	return *fbms << 10;
}

/* Adjust the real mode stack pointer.  We keep the real mode stack at
 * the top of free base memory, rather than allocating space for it.
 */

inline void adjust_real_mode_stack ( void ) {
	real_mode_stack = ( *fbms << 10 );
}

/* Allocate N bytes of base memory.  Amount allocated will be rounded
 * up to the nearest kB, since that's the granularity of the BIOS FBMS
 * counter.  Returns NULL if memory cannot be allocated.
 */

void * allot_base_memory ( size_t size ) {
	uint16_t size_kb = ( size + 1023 ) >> 10;
	void *ptr = NULL;

#ifdef DEBUG_BASEMEM
	printf ( "Trying to allocate %d kB of base memory, %d kB free\n",
		 size_kb, *fbms );
#endif

	/* Check available base memory */
	if ( size_kb > *fbms ) { return NULL; }

	/* Reduce available base memory */
	*fbms -= size_kb;

	/* Calculate address of memory allocated */
	ptr = phys_to_virt ( *fbms << 10 );

	/* Zero out memory.  We do this so that allocation of
	 * already-used space will show up in the form of a crash as
	 * soon as possible.
	 */
	memset ( ptr, 0, size_kb << 10 );

	/* Adjust real mode stack pointer */
	adjust_real_mode_stack ();

	return ptr;
}

/* Free base memory allocated by allot_base_memory.  The BIOS provides
 * nothing better than a LIFO mechanism for freeing memory (i.e. it
 * just has the single "total free memory" counter), but we improve
 * upon this slightly; as long as you free all the allotted blocks, it
 * doesn't matter what order you free them in.  (This will only work
 * for blocks that are freed via forget_base_memory()).
 *
 * Yes, it's annoying that you have to remember the size of the blocks
 * you've allotted.  However, since our granularity of allocation is
 * 1K, the alternative is to risk wasting the occasional kB of base
 * memory, which is a Bad Thing.  Really, you should be using as
 * little base memory as possible, so consider the awkwardness of the
 * API to be a feature! :-)
 */

void forget_base_memory ( void *ptr, size_t size ) {
	free_base_memory_block_t *free_block = NULL;
	uint16_t size_kb = ( size + 1023 ) >> 10;
	
	if ( ( ptr == NULL ) || ( size == 0 ) ) { return; }

	/* Mark this block as free */
	free_block = ( free_base_memory_block_t * ) ptr;
	free_block->magic = FREE_BLOCK_MAGIC;
	free_block->size_kb = size_kb;

	/* See if we're able to start releasing memory back to the BIOS */
	if ( phys_to_virt ( *fbms << 10 ) == free_block ) {
		/* Free all consecutive blocks marked as free */
		while ( free_block->magic == FREE_BLOCK_MAGIC ) {
			/* Return memory to BIOS */
			*fbms += free_block->size_kb;
#ifdef DEBUG_BASEMEM
			printf ( "Freed %d kB base memory, %d kB now free\n",
				 free_block->size_kb, *fbms );
#endif			

			/* Zero out freed block.  We do this in case
			 * the block contained any structures that
			 * might be located by scanning through
			 * memory.
			 */
			memset ( free_block, 0, free_block->size_kb << 10 );

			/* Stop processing if we're all the way up to 640K */
			if ( *fbms == BASE_MEMORY_MAX ) break;
			
			/* Calculate address of next potential free block */
			free_block = ( free_base_memory_block_t * )
				phys_to_virt ( *fbms << 10 );
		}
	}

	/* Adjust real mode stack pointer */
	adjust_real_mode_stack ();
}

#endif /* PCBIOS */
