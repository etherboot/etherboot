#include "etherboot.h"
#include <limits.h>

#if defined(RELOCATE)

/* by Eric Biederman */

/* within 1MB of ULONG_MAX is too close */

#define MAX_ADDR (ULONG_MAX - (1024*1024) +1)
void relocate(void)
{
	unsigned long addr, eaddr, size;
	int i;
	/* Walk through the memory map and find the highest address
	 * below 4GB that etherboot will fit into.
	 */
	/* First find the size of etherboot */
	addr = virt_to_phys(_text);
	eaddr = virt_to_phys(_end);
	size = (eaddr - addr + 0xf) & ~0xf;

	for(i = 0; i < meminfo.map_count; i++) {
		unsigned long r_start, r_end;
		unsigned long end;
		if (meminfo.map[i].type != E820_RAM) {
			continue;
		}
		if (meminfo.map[i].addr > MAX_ADDR) {
			continue;
		}
		if (meminfo.map[i].size > MAX_ADDR) {
			continue;
		}
		r_start = meminfo.map[i].addr;
		r_end = r_start + meminfo.map[i].size;
		if (r_end < r_start) {
			r_end = MAX_ADDR;
		}
		if (eaddr < r_end - size) {
			addr = r_end - size;
			eaddr = r_end;
		}
	}
	if (addr != virt_to_phys(_text)) {
		printf("Relocating _text from: [%x,%x) to [%x,%x)\n",
			virt_to_phys(_text), virt_to_phys(_end),
			addr, eaddr);
		relocate_to(addr);
	}
}

#endif /* RELOCATE */
