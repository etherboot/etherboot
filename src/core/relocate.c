#include "etherboot.h"

#if defined(RELOCATE)

/* by Eric Biederman */

/* On some platforms etherboot is compiled as a shared library, and we use
 * the ELF pic support to make it relocateable.  This works very nicely
 * for code, but since no one has implemented PIC data yet pointer
 * values in variables are a a problem.  Global variables are a
 * pain but the return addresses on the stack are the worst.  On these
 * platforms relocate_to will restart etherboot, to ensure the stack
 * is reinitialize and hopefully get the global variables
 * appropriately reinitialized as well.
 * 
 */

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

	/* If the current etherboot is beyond MAX_ADDR pretend it is
	 * at the lowest possible address.
	 */
	if (eaddr > MAX_ADDR) {
		eaddr = 0;
	}

	for(i = 0; i < meminfo.map_count; i++) {
		unsigned long r_start, r_end;
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
		printf("Relocating _text from: [%lx,%lx) to [%lx,%lx)\n",
			virt_to_phys(_text), virt_to_phys(_end),
			addr, eaddr);
		cleanup();
		relocate_to(addr);
	}
}

#endif /* RELOCATE */
