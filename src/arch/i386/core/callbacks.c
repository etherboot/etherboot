/* Callout/callback interface for Etherboot
 *
 * This file provides the mechanisms for making calls from Etherboot
 * to external programs and vice-versa.
 *
 * Initial version by Michael Brown <mbrown@fensystems.co.uk>, January 2004.
 */

#include "etherboot.h"
#include "callbacks.h"
#include <stdarg.h>

/* Wrapper function that takes a variable argument list and calls
 * v_ext_call with the resulting va_list.
 */
uint32_t _ext_call ( uint32_t address, ... ) {
	uint32_t ret;
	va_list ap;

	va_start ( ap, address );
	ret = v_ext_call ( address, ap );
	va_end ( ap );
	return ret;
}

#ifdef DEBUG_EXT_CALL

/* Note that printf() itself invokes putc(), which uses external
 * calls, therefore we must ensure that the old version of putc (using
 * _real_call) can be compiled back in to debug _ext_call!
 */

/* v_ext_call(): with debugging enabled, print the function trace if
 * directed to do so, then call to the _v_ext_call() assembly routine.
 * Without debugging enabled, expands to a macro that just calls
 * _v_ext_call() directly.
 */
uint32_t v_ext_call ( uint32_t address, va_list ap ) {
	va_list aq;
	uint32_t first_arg;
	
	va_copy ( aq, ap );
	first_arg = va_arg ( aq, typeof(first_arg) );
	if ( first_arg == EP_TRACE ) {
		v_ext_call_check ( address, aq );
		/* Pop first_arg from ap as well */
		va_arg ( ap, typeof(first_arg) );
	}
	va_end ( aq );
	_v_ext_call ( address, ap );
}

/* Print out a GDT segment descriptor.
 */
void print_gdt_segment ( uint16_t segment_num, gdt_segment_t *gdt_seg ) {
	printf ( "  Segment %hx base %x limit %x%s %sbit type %hhx\n",
		 segment_num,
		 GDT_SEGMENT_BASE(gdt_seg),
		 GDT_SEGMENT_LIMIT(gdt_seg),
		 ( GDT_SEGMENT_GRANULARITY(gdt_seg) == GDT_GRANULARITY_LARGE ?
		   " x 4 kb" : "" ),
		 ( GDT_SEGMENT_SIZE(gdt_seg) == GDT_SIZE_16BIT ? "16" :
		   ( GDT_SEGMENT_SIZE(gdt_seg) == GDT_SIZE_32BIT ? "32" :
		     ( GDT_SEGMENT_SIZE(gdt_seg) == GDT_SIZE_64BIT ? "64" :
		       "??" ) ) ),
		 GDT_SEGMENT_TYPE(gdt_seg)
		 );
#if 0 /* For checking that GDT interpretations are correct */
 {
	void *temp = (void*)gdt_seg;
	printf ( "    Raw %hx %hx %hhx %hhx %hhx %hhx\n",
		 *((uint16_t*)(temp+0)), *((uint16_t*)(temp+2)),
		 *((uint8_t*)(temp+4)), *((uint8_t*)(temp+5)),
		 *((uint8_t*)(temp+6)), *((uint8_t*)(temp+7)) );
 }
#endif
}

/* Print out parameter list for a call to ext_call().
 */
void v_ext_call_check ( uint32_t address, va_list ap ) {
	uint32_t type;

	printf ( "External call to %x with arguments:\n", address );
	/* Dump argument list */
	while ( ( type = va_arg ( ap, typeof(type) ) ) != EXTCALL_END_LIST ) {
		int valid = 0;

		switch ( type ) {
		case EXTCALL_NONE: {
			printf ( " Null argument\n" );
			valid = 1;
		} break;
		case EXTCALL_REGISTERS: {
			regs_t *regs;
			regs = va_arg ( ap, typeof(regs) );
			printf ( " Registers at %x:\n"
				 "  eax=%x, ebx=%x, ecx=%x, edx=%x,\n"
				 "  esp=%x, ebp=%x, esi=%x, edi=%x\n",
				 regs,
				 regs->eax, regs->ebx, regs->ecx, regs->edx,
				 regs->esp, regs->ebp, regs->esi, regs->edi );
			valid = 1;
		} break;
		case EXTCALL_SEG_REGISTERS: {
			seg_regs_t *seg_regs;
			seg_regs = va_arg ( ap, typeof(seg_regs) );
			printf ( " Segment registers at %x:\n"
				 "  cs=%hx ds=%hx ss=%hx es=%hx fs=%hx gs=%hx",
				 seg_regs,
				 seg_regs->cs, seg_regs->ds, seg_regs->ss,
				 seg_regs->es, seg_regs->fs, seg_regs->gs );
			valid = 1;
		} break;
		case EXTCALL_GDT: {
			GDT_STRUCT_t(0) *gdt;
			gdt_segment_t *gdt_seg;
			uint32_t eb_cs;
			gdt = va_arg ( ap, typeof(gdt) );
			printf ( " GDT at %x length %hx:\n", gdt,
				 gdt->limit + 1 );
			if ( gdt->address != virt_to_phys(gdt) ) {
				printf ( "  addr incorrect "
					 "(is %x, should be %hx)\n",
					 gdt->address,
					 virt_to_phys(gdt) );
			}
			for ( gdt_seg = &(gdt->segments[0]);
			      ( (void*)gdt_seg - (void*)gdt ) < gdt->limit ;
			      gdt_seg++ ) {
				print_gdt_segment ( (void*)gdt_seg-(void*)gdt,
						    gdt_seg );
			}
			eb_cs = va_arg ( ap, typeof(eb_cs) );
			printf ( "  Etherboot accessible via CS=%hx\n",
				 eb_cs );
			valid = 1;
		} break;
		case EXTCALL_STACK: {
			char *stack_base;
			uint32_t stack_length;
			uint32_t i;
			stack_base = va_arg ( ap, typeof(stack_base) );
			stack_length = va_arg ( ap, typeof(stack_length) );
			printf ( " Stack parameters at %x length %hx:\n  ",
				 stack_base, stack_length );
			for ( i = 0; i < stack_length; i++ ) {
				printf ( "%hhx ", stack_base[i] );
			}
			putchar ( '\n' );
			valid = 1;
		} break;
		case EXTCALL_RET_STACK: {
			char *ret_stack_base;
			uint32_t ret_stack_max;
			uint32_t *ret_stack_len;
			ret_stack_base = va_arg ( ap, typeof(ret_stack_base) );
			ret_stack_max = va_arg ( ap, typeof(ret_stack_max) );
			ret_stack_len = va_arg ( ap, typeof(ret_stack_len) );
			printf ( " Return stack at %x length %hx",
				 ret_stack_base, ret_stack_max );
			if ( ret_stack_len != NULL ) {
				printf ( " return length stored at %x",
					 ret_stack_len );
			}
			putchar ( '\n' );
			valid = 1;
		} break;
		case EXTCALL_RELOC_STACK: {
			char *reloc_stack;
			reloc_stack = va_arg ( ap, typeof(reloc_stack) );
			printf ( " Relocate stack to [......,%x)\n",
				 reloc_stack );
			valid = 1;
		} break;
		default:
			printf ( " Unknown argument type %hx", type );
			break;
		}
		if ( !valid ) {
			printf ( "Argument list error: aborting\n" );
			break;
		}
	}
	printf ( "End of argument list\n" );
}

#endif /* DEBUG_EXT_CALL */






/* Scrap routines used only during development of ext_call() and friends:
 */


extern int demo_extcall ( long beta, char gamma, char epsilon, long delta );
	/*	__asm__ ( "movl %%ebx, %%ecx;\n" : : ); */
/*	return beta + delta + gamma + epsilon; 
	}*/


int my_call ( char a, long b, char c, char e, long d );

int my_call ( char a, long b, char c, char e, long d ) {
	char temp_stack[256];
	int ret;
	regs_t registers;
	struct {
		uint16_t ret1;
		uint16_t ret2;
		uint16_t ret3;
		uint16_t ret4;
	} PACKED ret_parms;
	uint32_t ret_stack_len;

	registers.eax = a;
	registers.edx = 0x12345678;
	struct {
		uint16_t arg1;
		uint16_t arg2;
		uint16_t arg3;
	} PACKED test_parms = { 7, 8, 9 };

	GDT_STRUCT_t(6) gdt = {
		0, 0, 0, {
			GDT_SEGMENT_PMCS(virt_offset),
			GDT_SEGMENT_PMDS(virt_offset),
			GDT_SEGMENT_PMCS(0x1000),
			/*				GDT_SEGMENT_RMCS, */
			/*						GDT_SEGMENT_RMDS, */
			/*			GDT_SEGMENT_PMCS_PHYS,
						GDT_SEGMENT_PMDS_PHYS */
		}
	};
	GDT_ADJUST(&gdt);
	
	/*	ext_call_check ( demo_extcall, EP_STACK_PASSTHRU ( b, d ),
			 EP_GDT(&gdt),
			 EP_REGISTERS(&registers),
			 EP_STACK(&test_parms) ); */
	/*	return (int) ext_call ( demo_extcall, EP_STACK_PASSTHRU ( b, d ),
				EP_GDT(&gdt),
				EP_REGISTERS(&registers),
				EP_STACK(&test_parms) ); */
	registers.eax = 0x12344321;
	registers.ebx = 0xabcdabcd;
	/*	ext_call_check ( 
			 virt_to_phys(demo_extcall),
			 EP_REGISTERS(&registers),
			 EP_STACK_PASSTHRU(b, d), EP_GDT(&gdt, 0x18),
			 EP_RELOC_STACK(((void*)temp_stack) + 256),
			 EP_RET_VARSTACK( &ret_parms, &ret_stack_len ) ); */

	memset ( temp_stack, 0, 256 );

	ret = (int) ext_call_trace ( virt_to_phys(demo_extcall) - 0x1000,
			       /* demo_extcall, */
			       EP_REGISTERS(&registers),
			       EP_STACK_PASSTHRU(b, d), EP_GDT(&gdt, 0x18),
			       EP_RELOC_STACK(((void*)temp_stack) + 256),
			       EP_RET_VARSTACK( &ret_parms, &ret_stack_len ) );
	/*	ret = (int) ext_call ( demo_extcall, EP_STACK_PASSTHRU ( b, d ) ,
		EP_REGISTERS(&registers) ); */
	printf ( "ecx = %#x\n", registers.ecx );

	printf ( "return stack length %hx: %hx %hx %hx %hx\n",
		 ret_stack_len,
		 ret_parms.ret1, ret_parms.ret2,
		 ret_parms.ret3, ret_parms.ret4 );
	/*		hex_dump(temp_stack,256);  */
	return ret;
}

