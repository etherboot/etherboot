#ifdef PCBIOS

#include "etherboot.h"
#include "realmode.h"

#define CF ( 1 << 0 )

#define MEMSIZES_DEBUG 0

/* by Eric Biederman */

struct meminfo meminfo;

/**************************************************************************
BASEMEMSIZE - Get size of the conventional (base) memory
**************************************************************************/
unsigned short basememsize ( void ) {
	BEGIN_RM_FRAGMENT(rm_basememsize);
	__asm__ ( "int $0x12" );
	END_RM_FRAGMENT(rm_basememsize);
	return real_call ( rm_basememsize, NULL, NULL );
}

/**************************************************************************
MEMSIZE - Determine size of extended memory
**************************************************************************/
unsigned int memsize ( void ) {
	struct {
		reg16_t ax;
	} PACKED in_stack;
	struct {
		reg16_t flags;
		reg16_t ax;
		reg16_t bx;
		reg16_t cx;
		reg16_t dx;
	} PACKED out_stack;
	int memsize;

	BEGIN_RM_FRAGMENT(rm_memsize);
	/* Some buggy BIOSes don't clear/set carry on pass/error of
	 * e801h memory size call or merely pass cx,dx through without
	 * changing them, so we set carry and zero cx,dx before call.
	 */
	__asm__ ( "stc" );
	__asm__ ( "xorw %cx,%cx" );
	__asm__ ( "xorw %dx,%dx" );
	__asm__ ( "popw %ax" );
	__asm__ ( "int $0x15" );
	__asm__ ( "pushfw" );
	__asm__ ( "pushw %dx" );
	__asm__ ( "pushw %cx" );
	__asm__ ( "pushw %bx" );
	__asm__ ( "pushw %ax" );
	END_RM_FRAGMENT(rm_memsize);

	/* Try INT 15,e801 first */
	in_stack.ax.word = 0xe801;
	real_call ( rm_memsize, &in_stack, &out_stack );
	if ( out_stack.flags.word & CF ) {
		/* INT 15,e801 not supported: try INT 15,88 */
		in_stack.ax.word = 0x8800;
		memsize = real_call ( rm_memsize, &in_stack, &out_stack );
	} else {
		/* Some BIOSes report extended memory via ax,bx rather
		 * than cx,dx
		 */
		if ( (out_stack.cx.word==0) && (out_stack.dx.word==0) ) {
			/* Use ax,bx */
			memsize = ( out_stack.bx.word<<6 ) + out_stack.ax.word;
		} else {
			/* Use cx,dx */
			memsize = ( out_stack.dx.word<<6 ) + out_stack.cx.word;
		}
	}
	return memsize;
}

#define SMAP ( 0x534d4150 )
int meme820 ( struct e820entry *buf, int count ) {
	struct {
		reg16_t flags;
		reg32_t eax;
		reg32_t ebx;
		struct e820entry entry;
	} PACKED stack;
	int index = 0;
	
	BEGIN_RM_FRAGMENT(rm_meme820);
	__asm__ ( "addw $6, %sp" );	/* skip flags, eax */
	__asm__ ( "popl %ebx" );
	__asm__ ( "pushw %ss" );	/* es:di = ss:sp */
	__asm__ ( "popw %es" );
	__asm__ ( "movw %sp, %di" );
	__asm__ ( "movl $0xe820, %eax" );
	__asm__ ( "movl %0, %%edx" : : "i" ( SMAP ) );
	__asm__ ( "movl %0, %%ecx" : : "i" ( sizeof(stack.entry) ) );
	__asm__ ( "int $0x15" );
	__asm__ ( "pushl %ebx" );
	__asm__ ( "pushl %eax" );
	__asm__ ( "pushfw" );
	END_RM_FRAGMENT(rm_meme820);

	stack.ebx.dword = 0; /* 'EOF' marker */
	while ( ( index < count ) &&
		( ( index == 0 ) || ( stack.ebx.dword != 0 ) ) ) {
		real_call ( rm_meme820, &stack, &stack );
		if ( stack.eax.dword != SMAP ) return 0;
		if ( stack.flags.word & CF ) return 0;
		buf[index++] = stack.entry;
	}
	return index;
}

void get_memsizes(void)
{
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
#if MEMSIZES_DEBUG
{
	int i;
	printf("basememsize %d\n", meminfo.basememsize);
	printf("memsize %d\n",     meminfo.memsize);
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
}
#endif
	/* Allocate the real mode stack */
	adjust_real_mode_stack ();
}

#endif /* PCBIOS */
