#include "etherboot.h"

size_t heap_ptr, heap_top, heap_bot;

void init_heap(void)
{
	size_t size;
	size_t start, end;
	int i;
	/* Find the largest contiguous area of memory that
	 * I can use for the heap, which is organized as 
	 * a stack that grows backwards through memory.
	 */
	start = virt_to_phys(_text);
	end  = virt_to_phys(_end);
	size = 0;
	for(i = 0; i < meminfo.map_count; i++) {
		unsigned long r_start, r_end;
		if (meminfo.map[i].type != E820_RAM)
			continue;
		if (meminfo.map[i].addr > ULONG_MAX)
			continue;
		if (meminfo.map[i].size > ULONG_MAX)
			continue;
		
		r_start = meminfo.map[i].addr;
		r_end = r_start + meminfo.map[i].size;
		if (r_end < r_start) {
			r_end = ULONG_MAX;
		}
		/* If the area overlaps etherboot split the area */
		if ((end > r_start) && (start < r_end)) {
			/* Keep the larger piece */
			if ((r_end - end) >= (r_start - start)) {
				r_start = end;
			} else {
				r_end = start;
			}
		}
		if (r_end - r_start > size) {
			size = r_end - r_start;
			heap_top = r_start;
			heap_bot = r_end;
		}
	}
	heap_ptr = heap_bot;
}

void *allot(size_t size)
{
	void *ptr;
	size_t *mark, addr;
	/* Get an 8 byte aligned chunk of memory off of the heap 
	 * An extra sizeof(size_t) bytes is allocated to track
	 * the size of the object allocated on the heap.
	 */
	addr = (heap_ptr - (size + sizeof(size_t))) &  ~7;
	if (addr < heap_top) {
		ptr = 0;
	} else {
		mark = phys_to_virt(addr);
		*mark = size;
		heap_ptr = addr;
		ptr = phys_to_virt(addr + sizeof(size_t));
	}
	return ptr;
}

void forget(void *ptr)
{
	size_t *mark, addr;
	size_t size;

	if (!ptr) {
		return;
	}
	addr = virt_to_phys(ptr);
	mark = phys_to_virt(addr - sizeof(size_t));
	size = *mark;
	addr += (size + 7) & ~7;
	
	if (addr > heap_bot) {
		addr = heap_bot;
	}
	heap_ptr = addr;
}
