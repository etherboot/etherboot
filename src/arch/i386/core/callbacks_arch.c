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

/*****************************************************************************
 *
 * IN_CALL INTERFACE
 *
 *****************************************************************************
 */

/* in_call(): entry point for calls in to Etherboot from external code. 
 *
 * Parameters: some set up by assembly code _in_call(), others as
 * passed from external code.
 */
uint32_t i386_in_call ( va_list ap, i386_pm_in_call_data_t pm_data,
			uint32_t opcode ) {

	i386_rm_in_call_data_t rm_data;
	in_call_data_t in_call_data = { &pm_data, &rm_data };

	/* Fill out rm_data if we were called from real mode */
	if ( opcode & EB_CALL_FROM_REAL_MODE ) {
		rm_data = va_arg ( ap, typeof(rm_data) );
	}
	
	/* Hand off to main in_call() routine */
	return in_call ( opcode, ap, &in_call_data );
}

/* install_rm_callback_interface(): install real-mode callback
 * interface at specified address.
 *
 * Real-mode code may then call to this address (or lcall to this
 * address plus RM_IN_CALL_FAR) in order to make an in_call() to
 * Etherboot.
 *
 * Returns the size of the installed code, or 0 if the code could not
 * be installed.
 */
int install_rm_callback_interface ( void *address, size_t available ) {
	if ( available &&
	     ( available < rm_callback_interface_size ) ) return 0;

	/* Inform RM code where to find Etherboot */
	rm_etherboot_location = virt_to_phys(_text);

	/* Install callback interface */
	memcpy ( address, &rm_callback_interface,
		 rm_callback_interface_size );

	return rm_callback_interface_size;
}

/*****************************************************************************
 *
 * EXT_CALL INTERFACE
 *
 *****************************************************************************
 */

/* Debugging for i386-specific ext_call parameters
 */
#ifdef DEBUG_EXT_CALL

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

int v_arch_ec_check_param ( int type, va_list *ap ) {

	switch ( type ) {
	case EXTCALL_REGISTERS: {
		regs_t *regs;
		regs = va_arg ( *ap, typeof(regs) );
		printf ( " Registers at %x:\n"
			 "  eax=%x, ebx=%x, ecx=%x, edx=%x,\n"
			 "  esp=%x, ebp=%x, esi=%x, edi=%x\n",
			 regs,
			 regs->eax, regs->ebx, regs->ecx, regs->edx,
			 regs->esp, regs->ebp, regs->esi, regs->edi );
	} break;
	case EXTCALL_SEG_REGISTERS: {
		seg_regs_t *seg_regs;
		seg_regs = va_arg ( *ap, typeof(seg_regs) );
		printf ( " Segment registers at %x:\n"
			 "  cs=%hx ds=%hx ss=%hx"
			 " es=%hx fs=%hx gs=%hx\n",
			 seg_regs,
				 seg_regs->cs, seg_regs->ds, seg_regs->ss,
			 seg_regs->es, seg_regs->fs, seg_regs->gs );
	} break;
	case EXTCALL_GDT: {
		GDT_STRUCT_t(0) *gdt;
		gdt_segment_t *gdt_seg;
		uint32_t eb_cs;
		gdt = va_arg ( *ap, typeof(gdt) );
		printf ( " GDT at %x length %hx:\n", gdt,
			 gdt->descriptor.limit + 1 );
		if ( gdt->descriptor.address != virt_to_phys(gdt) ) {
			printf ( "  addr incorrect "
				 "(is %x, should be %hx)\n",
				 gdt->descriptor.address,
				 virt_to_phys(gdt) );
		}
		for ( gdt_seg = &(gdt->segments[0]);
		      ( (void*)gdt_seg - (void*)gdt )
			      < gdt->descriptor.limit ;
		      gdt_seg++ ) {
			print_gdt_segment ( (void*)gdt_seg-(void*)gdt,
					    gdt_seg );
		}
		eb_cs = va_arg ( *ap, typeof(eb_cs) );
		printf ( "  Etherboot accessible via CS=%hx\n",
			 eb_cs );
	} break;
	default:
		return 0;
		break;
	}
	return 1;
}

#endif /* DEBUG_EXT_CALL */


/* Scrap routines used only during development of ext_call() and friends:
 */


extern int demo_extcall ( long beta, char gamma, char epsilon, long delta );
	/*	__asm__ ( "movl %%ebx, %%ecx;\n" : : ); */
/*	return beta + delta + gamma + epsilon; 
	}*/
extern void _in_call ( void );
extern void _in_call_far ( void );

extern void demo_extcall_end;

extern void hello_world;
extern void ehello_world;
extern void trial_real_incall;
extern void etrial_real_incall;



extern int get_esp ( void );

int test_extcall ( int a, int b, int c, int d, int e ) {
	/*		char temp_stack[256]; */
	int ret;
	regs_t registers;
	struct {
		uint16_t ret1;
		uint16_t ret2;
		uint16_t ret3;
		uint16_t ret4;
	} PACKED ret_parms;
	uint32_t ret_stack_len;
	seg_regs_t seg_regs;

	registers.eax = 2;
	registers.edx = 0x12345678;

	memset ( &seg_regs, 0, sizeof(seg_regs) );
	seg_regs.cs = 0x18;
	seg_regs.ss = 0x20;
	seg_regs.ds = 0x20;
	seg_regs.es = 0x20;
	seg_regs.fs = 0x20;
	seg_regs.gs = 0x20;

	struct {
		uint16_t arg1;
		uint16_t arg2;
		uint16_t arg3;
	} PACKED test_parms = { 7, 8, 9 };

	uint16_t rm_seg = 0x7c0;

	GDT_STRUCT_t(6) gdt = {
		{ 0, 0, 0 }, {
			GDT_SEGMENT_PMCS(virt_offset),
			GDT_SEGMENT_PMDS(virt_offset),
			GDT_SEGMENT_PMCS_PHYS,
			GDT_SEGMENT_PMDS_PHYS,
			GDT_SEGMENT_RMCS(rm_seg << 4),
			GDT_SEGMENT_RMDS(rm_seg << 4),
			/*			GDT_SEGMENT_PMCS(virt_offset),*/
			/*			GDT_SEGMENT_PMDS(virt_offset), */
			/*			GDT_SEGMENT_RMCS,
						GDT_SEGMENT_RMDS, */
		}
	};
	GDT_ADJUST(&gdt);
	
	registers.eax = 0x12344321;
	registers.ebx = 0xabcdabcd;

	/*		memset ( temp_stack, 0, 256 ); */
	seg_regs_t rm_seg_regs =
		{ rm_seg, rm_seg, rm_seg, rm_seg, rm_seg, rm_seg };

	ret = (int) ext_call (
	   0, /* virt_to_phys(demo_extcall), */
	   EP_REGISTERS(&registers),
	   EP_SEG_REGISTERS(&seg_regs),
	   EP_STACK(&rm_seg_regs),
	   EP_STACK_PASSTHRU(b, d),
	   EP_GDT(&gdt, 0x18),
	   EP_RELOC_STACK((rm_seg << 4 ) + 0x1000),
	   EP_RET_VARSTACK( &ret_parms, &ret_stack_len ),
	   EP_TRAMPOLINE(_prot_to_real, _prot_to_real_end ),
	   EP_TRAMPOLINE(&hello_world, &ehello_world ),
	   EP_TRAMPOLINE(_real_to_prot, _real_to_prot_end )
	   );
	/* When making a real call, relocate the stack only if
	 * necessary.  (This requires ext_call to allow segment-adjust
	 * of %esp, as it does for %eip).
	 *
	 * As well as saving on relocation, this should solve the
	 * problem of only having one "internal" real-mode stack: the
	 * only time it's a problem is if we try to make two
	 * concurrent real-mode calls, which is only likely to happen
	 * if a real-mode routine that we called out to calls back to
	 * us.  However, in this case the stack is already in base
	 * memory and so we can just use it where it is! :)
	 */
	
	printf ( "ecx = %#x (%#x)\n", registers.ecx,
		 phys_to_virt(registers.ecx) );

	/*	hex_dump(((void*)temp_stack),256); */

	seg_regs.cs = 0x18;
	seg_regs.ss = 0x20;
	seg_regs.ds = 0x20;
	seg_regs.es = 0x20;
	seg_regs.fs = 0x20;
	seg_regs.gs = 0x20;
	printf ( "_in_call_far is at %x\n", virt_to_phys ( _in_call_far ) );
	printf ( "in_call is at %x\n", virt_to_phys ( in_call ) );

	struct {
		uint32_t opcode;
		uint32_t api;
	} testing;
	testing.opcode = EB_OPCODE_PXE;
	testing.api = 0x200;
	registers.bx = 0xabcd;
	ret = (int) ext_call (
	    virt_to_phys ( _in_call_far ),
	    EP_REGISTERS(&registers),
	    EP_SEG_REGISTERS(&seg_regs),
	    EP_GDT(&gdt, 0x18),
	    EP_RELOC_STACK(0x9fa00),
	    EP_STACK(&testing)
	    );



	seg_regs.cs = 0x18;
	seg_regs.ss = 0x20;
	seg_regs.ds = 0x20;
	seg_regs.es = 0x20;
	seg_regs.fs = 0x20;
	seg_regs.gs = 0x20;


	/* Install real-mode callback interface at 0x80000
	 */
	uint32_t target = 0x80000;
	install_rm_callback_interface ( phys_to_virt(target), 0 );
	/*	rm_etherboot_location = virt_to_phys(_text);
		memcpy ( phys_to_virt ( target ), &rm_callback_interface,
		rm_callback_interface_size ); */
	

	ret = (int) ext_call (
	   0, /* virt_to_phys(demo_extcall), */
	   EP_REGISTERS(&registers),
	   EP_SEG_REGISTERS(&seg_regs),
	   EP_STACK(&rm_seg_regs),
	   EP_STACK_PASSTHRU(b, d),
	   EP_GDT(&gdt, 0x18),
	   EP_RELOC_STACK((rm_seg << 4 ) + 0x1000),
	   EP_RET_STACK( &rm_seg_regs ),
	   EP_RET_VARSTACK( &ret_parms, &ret_stack_len ),
	   EP_TRAMPOLINE(_prot_to_real, _prot_to_real_end ),
	   EP_TRAMPOLINE(&trial_real_incall, &etrial_real_incall ),
	   EP_TRAMPOLINE(_real_to_prot, _real_to_prot_end )
	   );

	printf ( "return stack length %hx: %hx %hx %hx %hx\n",
		 ret_stack_len,
		 ret_parms.ret1, ret_parms.ret2,
		 ret_parms.ret3, ret_parms.ret4 );

	return ret;
}

