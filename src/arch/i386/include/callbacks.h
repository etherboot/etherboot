/* Callout/callback interface for Etherboot
 *
 * This file provides the mechanisms for making calls from Etherboot
 * to external programs and vice-versa.
 *
 * Initial version by Michael Brown <mbrown@fensystems.co.uk>, January 2004.
 *
 * $Id$
 */

#ifndef CALLBACKS_H
#define CALLBACKS_H

/* Types of data that can be passed to external_call().
 */
#define EXTCALL_NONE		(0x0000UL)
#define EXTCALL_REGISTERS	(0x0001UL)
#define EXTCALL_SEG_REGISTERS	(0x0002UL)
#define EXTCALL_GDT		(0x0003UL)
#define EXTCALL_STACK		(0x0004UL)
#define EXTCALL_STACK_CORRUPT	(0x0005UL)
#define EXTCALL_END_LIST	(0xffffUL)

/* Struct to hold general-purpose register values.  PUSHAL and POPAL
 * can work directly with this structure; do not change the order of
 * registers.
 */
typedef struct {
	uint32_t edi;
	uint32_t esi;
	uint32_t ebp;
	uint32_t esp;
	uint32_t ebx;
	uint32_t edx;
	uint32_t ecx;
	uint32_t eax;
} regs_t;
/* As part of an external_call parameter list */
#define REGISTERS(regs) EXTCALL_REGISTERS, (regs)

/* Struct to hold segment register values.  Use as part of a GDT()
 * structure.
 */
typedef struct {
	uint16_t cs;
	uint16_t ds;
	uint16_t ss;
	uint16_t es;
	uint16_t fs;
	uint16_t gs;
} PACKED seg_regs_t;
/* As part of an external_call parameter list */
#define SEG_REGISTERS(seg_regs) EXTCALL_SEG_REGISTERS, (seg_regs)

/* Struct for a GDT entry.  Use GDT_SEGMENT() to fill it in.
 */
typedef struct {
	uint16_t limit_0_15;
	uint16_t base_0_15;
	uint8_t base_16_23;
	uint8_t accessed__type__sflag__dpl__present;
	uint8_t limit_16_19__avl__size__granularity;
	uint8_t base_24_31;
} PACKED gdt_segment_t;

#define GDT_SEGMENT(base,limit,type,sflag,dpl,avl,size,granularity) \
	( (gdt_segment_t) { \
		( (limit) & 0xffff ), \
		( (base) & 0xffff ), \
		( ( (base) >> 16 ) & 0xff ), \
		( ( 1 << 0 ) | ( (type) << 1 ) | \
		  ( (sflag) << 4 ) | ( (dpl) << 5 ) | ( 1 << 7 ) ), \
		( ( (limit) >> 16 ) | \
		  ( (avl) << 4 ) | ( (size) << 5 ) | ( (granularity) << 7 ) ),\
		( (base) >> 24 ) \
	} )
#define GDT_SEGMENT_BASE(gdt_segment) \
	( (gdt_segment)->base_0_15 | \
	  (gdt_segment)->base_16_23 << 16 | \
	  (gdt_segment)->base_24_31 << 24 )
#define GDT_SEGMENT_LIMIT(gdt_segment) \
	( (gdt_segment)->limit_0_15 | \
	  ( ( (gdt_segment)->limit_16_19__avl__size__granularity \
	      & 0xf ) << 16 ) )
#define GDT_SEGMENT_GRANULARITY(gdt_segment) \
	( ( (gdt_segment)->limit_16_19__avl__size__granularity \
	    & 0x80 ) >> 7 )
#define GDT_SEGMENT_TYPE(gdt_segment) \
	( ( (gdt_segment)->accessed__type__sflag__dpl__present & 0x0e ) >> 1 )
#define GDT_SEGMENT_SIZE(gdt_segment) \
	( ( (gdt_segment)->limit_16_19__avl__size__granularity \
	    & 0x60 ) >> 5 )

#define GDT_TYPE_DATA			(0x0)
#define GDT_TYPE_STACK			(0x2)
#define GDT_TYPE_WRITEABLE		(0x1)
#define GDT_TYPE_CODE			(0x6)
#define GDT_TYPE_EXEC_ONLY_CODE		(0x4)
#define GDT_TYPE_CONFORMING		(0x1)
#define GDT_SFLAG_SYSTEM		(0)
#define GDT_SFLAG_NORMAL		(1)
#define GDT_AVL_NORMAL			(0)
#define GDT_SIZE_16BIT			(0x0)
#define GDT_SIZE_32BIT			(0x2)
#define GDT_SIZE_64BIT			(0x1)
#define GDT_SIZE_UNKNOWN		(0x3)
#define GDT_GRANULARITY_SMALL		(0)
#define GDT_GRANULARITY_LARGE		(1)
#define GDT_SEGMENT_NORMAL(base,limit,type,size,granularity) \
	GDT_SEGMENT ( base, limit, type, \
		      GDT_SFLAG_NORMAL, 0, GDT_AVL_NORMAL, \
		      size, granularity )

/* Protected mode code segment */
#define GDT_SEGMENT_PMCS(base) GDT_SEGMENT_NORMAL ( \
	base, 0xfffff, GDT_TYPE_CODE | GDT_TYPE_CONFORMING, \
	GDT_SIZE_32BIT, GDT_GRANULARITY_LARGE )
#define GDT_SEGMENT_PMCS_PHYS GDT_SEGMENT_PMCS(0)
/* Protected mode data segment */
#define GDT_SEGMENT_PMDS(base) GDT_SEGMENT_NORMAL ( \
	base, 0xfffff, GDT_TYPE_DATA | GDT_TYPE_WRITEABLE, \
	GDT_SIZE_32BIT, GDT_GRANULARITY_LARGE )
#define GDT_SEGMENT_PMDS_PHYS GDT_SEGMENT_PMDS(0)
/* Real mode code segment */
/* Not sure if there's any reason to use GDT_TYPE_EXEC_ONLY_CODE
 * instead of just GDT_TYPE_CODE, but that's what our old GDT did and
 * it worked, so I'm not changing it.
 */
#define GDT_SEGMENT_RMCS GDT_SEGMENT_NORMAL ( \
	0, 0xffff, GDT_TYPE_EXEC_ONLY_CODE | GDT_TYPE_CONFORMING, \
	GDT_SIZE_16BIT, GDT_GRANULARITY_SMALL )
/* Real mode data segment */
#define GDT_SEGMENT_RMDS GDT_SEGMENT_NORMAL ( \
	0, 0xffff, GDT_TYPE_DATA | GDT_TYPE_WRITEABLE, \
	GDT_SIZE_16BIT, GDT_GRANULARITY_SMALL )
/* Long mode code segment */
#define GDT_SEGMENT_LMCS(base) GDT_SEGMENT_NORMAL ( \
	base, 0xfffff, GDT_TYPE_CODE | GDT_TYPE_CONFORMING, \
	GDT_SIZE_64BIT, GDT_GRANULARITY_LARGE )
#define GDT_SEGMENT_LMCS_PHYS GDT_SEGMENT_LMCS(0)
/* Long mode data segment */
/* AFIACT, GDT_SIZE_64BIT applies only to code segments */
#define GDT_SEGMENT_LMDS(base) GDT_SEGMENT_NORMAL ( \
	base, 0xfffff, GDT_TYPE_DATA | GDT_TYPE_WRITEABLE, \
	GDT_SIZE_32BIT, GDT_GRANULARITY_LARGE )
#define GDT_SEGMENT_LMDS_PHYS GDT_SEGMENT_LMDS(0)

/* Template for creating GDT structures (including segment register
 * lists), suitable for passing as parameters to external_call().
 */
#define GDT_STRUCT_t(num_segments) \
	struct { \
		uint16_t limit; \
		uint32_t address; \
		uint16_t padding; \
		gdt_segment_t segments[num_segments]; \
	} PACKED
/* And utility function for filling it in */
#define GDT_ADJUST(structure) { \
	(structure)->address = virt_to_phys(&((structure)->limit)); \
	(structure)->limit = sizeof((structure)->segments) + 8 - 1; \
	(structure)->padding = 0; \
}
/* As part of an external_call parameter list */
#define GDT(structure) EXTCALL_GDT, &((structure)->limit)

/* Stack alignment used by GCC.  Must be a power of 2.
 */
#define STACK_ALIGNMENT 4

/* GCC seems to sometimes copy a parameter off the call stack to a new
 * temporary area (also on the stack) if you use the & operator to
 * find its address.  This seems not to occur if you simply use the
 * value of the parameter rather than its address.  It doesn't seem to
 * affect parameters whose size is a multiple of STACK_ALIGNMENT.  The
 * macro WILL_BE_LEFT_ON_STACK(x) evalutes to true if this is the
 * case, i.e. if taking &x will *not* cause x to be copied to a new
 * location, i.e. if &x will be the actual address of the original x
 * on the stack.
 */
#define WILL_BE_LEFT_ON_STACK(x) ( ( sizeof((x)) % STACK_ALIGNMENT ) == 0 )

/* As part of an external_call parameter list.
 *
 * This will automatically check to see that "first" and "last" won't
 * suffer from the "&" problem mentioned above; if they do then
 * EXTCALL_STACK_CORRUPT will be passed as the parameter type.
 */
#define STACK_PASSTHRU(first,last) \
	( WILL_BE_LEFT_ON_STACK((first)) && WILL_BE_LEFT_ON_STACK((last)) ? \
	  EXTCALL_STACK : EXTCALL_STACK_CORRUPT ), \
	(void*)&(first), \
	( ( (void*)&(last) + sizeof((last)) - (void*)&(first) \
	    + (STACK_ALIGNMENT-1) ) & ~(STACK_ALIGNMENT-1) )

/* Function prototypes */
extern uint32_t _ext_call ( uint32_t address, ... );
#ifdef DEBUG_EXT_CALL
extern uint32_t _ext_call_check ( uint32_t address, ... );
#define ext_call_check(...) _ext_call_check( __VA_ARGS__, EXTCALL_END_LIST )
#else
#define ext_call(...) _ext_call( __VA_ARGS__, EXTCALL_END_LIST )
#endif

#endif /* CALLBACKS_H */
