#ifdef PCBIOS

#include "etherboot.h"

/* by Eric Biederman */

extern unsigned int memsize P((void));
extern unsigned short basememsize P((void));
extern int meme820(struct e820entry *buf, int count);

struct meminfo meminfo;

void get_memsizes(void)
{
	int i;
	meminfo.basememsize = basememsize();
	meminfo.memsize = memsize();
	meminfo.map_count = meme820(meminfo.map, E820MAX);
	if (meminfo.map_count == 0) {
		/* If we don't have an e820 memory map fake it */
		meminfo.map_count = 2;
		meminfo.map[0].addr = 0;
		meminfo.map[0].size = meminfo.basememsize << 10;
		meminfo.map[0].type = E820_RAM;
		meminfo.map[1].addr = 1024*1024;
		meminfo.map[1].size = meminfo.memsize << 10;
		meminfo.map[1].type = E820_RAM;
	}
	/* Allocate the real mode stack */
	real_mode_stack = (meminfo.basememsize << 10);
}

#endif
