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
#ifndef IGNORE_E820_MAP
	meminfo.map_count = meme820(meminfo.map, E820MAX);
#else
	meminfo.map_count = 0;
#endif
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
#if 0
	printf("basememsize %d\n", meminfo.basememsize);
	printf("memsize %d\n",     meminfo.basememsize);
	printf("Memory regions(%d):\n", meminfo.map_count);
	for(i = 0; i < meminfo.map_count; i++) {
		unsigned long long r_start, r_end;
		r_start = meminfo.map[i].addr;
		r_end = r_start + meminfo.map[i].size;
		printf("[%X%X, %X%X) type %d\n", 
			(unsigned long)(r_start >> 32),
			(unsigned long)r_start,
			(unsigned long)(r_end >> 32),
			(unsigned long)r_end,
			meminfo.map[i].type);
		sleep(1); /* No way to see 32 entries on a standard 80x25 screen... */
	}
#endif
	/* Allocate the real mode stack */
	real_mode_stack = (meminfo.basememsize << 10);
}

#endif /* PCBIOS */
