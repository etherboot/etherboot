/* PXE callback mechanisms.  This file contains only the portions
 * specific to i386: i.e. the low-level mechanisms for calling in from
 * an NBP to the PXE stack and for starting an NBP from the PXE stack.
 */

#ifdef PXE_EXPORT

#include "etherboot.h"
#include "callbacks.h"
#include "pxe.h"
#include "pxe_callbacks.h"
#include "pxe_export.h"
#include <stdarg.h>

/* Overall PXE stack size
 */
static inline int pxe_stack_size ( void ) {
	return sizeof(pxe_stack_t)
		+ pxe_callback_interface_size
		+ rm_callback_interface_size;
}

/* Utility routine: byte checksum
 */
uint8_t byte_checksum ( void *address, size_t size ) {
	unsigned int i, sum = 0;

	for ( i = 0; i < size; i++ ) {
		sum += ((uint8_t*)address)[i];
	}
	return (uint8_t)sum;
}

/* install_pxe_stack(): install PXE stack.
 * 
 * Use base = NULL for auto-allocation of base memory
 */
pxe_stack_t * install_pxe_stack ( void *base ) {
	pxe_t *pxe;
	pxenv_t *pxenv;
	void *pxe_callback_code;
	void (*pxe_in_call_far)(void);
	void (*pxenv_in_call_far)(void);
	void *rm_callback_code;
	void *end;

	/* If already installed, just return */
	if ( pxe_stack != NULL ) return pxe_stack;

	/* Allocate base memory if requested to do so
	 */
	if ( base == NULL ) {
		base = allot_base_memory ( pxe_stack_size() );
		if ( base == NULL ) return NULL;
	}

	/* Round address up to 16-byte physical alignment */
	pxe_stack = (pxe_stack_t *)
		( phys_to_virt ( ( virt_to_phys(base) + 0xf ) & ~0xf ) );
	
	/* Calculate addresses for portions of the stack */
	pxe = &(pxe_stack->pxe);
	pxenv = &(pxe_stack->pxenv);
	pxe_callback_code = &(pxe_stack->arch_data);
	pxe_in_call_far = _pxe_in_call_far +  
		( pxe_callback_code - &pxe_callback_interface );
	pxenv_in_call_far = _pxenv_in_call_far +
		( pxe_callback_code - &pxe_callback_interface );
	rm_callback_code = pxe_callback_code + pxe_callback_interface_size;
	end = rm_callback_code + rm_callback_interface_size;

	/* Initialise !PXE data structures */
	memcpy ( pxe->Signature, "!PXE", 4 );
	pxe->StructLength = sizeof(*pxe);
	pxe->StructRev = 0;
	pxe->reserved_1 = 0;
	/* We don't yet have an UNDI ROM ID structure */
	pxe->UNDIROMID.segment = 0;
	pxe->UNDIROMID.offset = 0;
	/* or a BC ROM ID structure */
	pxe->BaseROMID.segment = 0;
	pxe->BaseROMID.offset = 0;
	pxe->EntryPointSP.segment = SEGMENT(pxe_stack);
	pxe->EntryPointSP.offset = (void*)pxe_in_call_far - (void*)pxe_stack;
	/* No %esp-compatible entry point yet */
	pxe->EntryPointESP.segment = 0;
	pxe->EntryPointESP.offset = 0;
	pxe->StatusCallout.segment = -1;
	pxe->StatusCallout.offset = -1;
	pxe->reserved_2 = 0;
	pxe->SegDescCn = 7;
	pxe->FirstSelector = 0;
	/* Currently we use caller's stack */
	pxe->Stack.Seg_Addr = 0;
	pxe->Stack.Phy_Addr = 0;
	pxe->Stack.Seg_Size = 0;
	/* We specify that code and data segments are the same, for now */
	pxe->UNDIData.Seg_Addr = SEGMENT(pxe_stack);
	pxe->UNDIData.Phy_Addr = virt_to_phys(pxe_stack);
	pxe->UNDIData.Seg_Size = end - (void*)pxe_stack;
	pxe->UNDICode.Seg_Addr = SEGMENT(pxe_stack);
	pxe->UNDICode.Phy_Addr = virt_to_phys(pxe_stack);
	pxe->UNDICode.Seg_Size = end - (void*)pxe_stack;
	/* No base code loaded */
	pxe->BC_Data.Seg_Addr = 0;
	pxe->BC_Data.Phy_Addr = 0;
	pxe->BC_Data.Seg_Size = 0;
	pxe->BC_Code.Seg_Addr = 0;
	pxe->BC_Code.Phy_Addr = 0;
	pxe->BC_Code.Seg_Size = 0;
	pxe->BC_CodeWrite.Seg_Addr = 0;
	pxe->BC_CodeWrite.Phy_Addr = 0;
	pxe->BC_CodeWrite.Seg_Size = 0;
	pxe->StructCksum -= byte_checksum ( pxe, sizeof(*pxe) );

	/* Initialise PXENV+ data structures */
	memcpy ( pxenv->Signature, "PXENV+", 6 );
	pxenv->Version = 0x201;
	pxenv->Length = sizeof(*pxenv);
	pxenv->RMEntry.segment = SEGMENT(pxe_stack);
	pxenv->RMEntry.offset = (void*)pxenv_in_call_far - (void*)pxe_stack;
	pxenv->PMOffset = 0; /* "Do not use" says the PXE spec */
	pxenv->PMSelector = 0; /* "Do not use" says the PXE spec */
	pxenv->StackSeg = 0; /* Currently we use caller's stack */
	pxenv->StackSize = 0;
	pxenv->BC_CodeSeg = 0;
	pxenv->BC_CodeSize = 0;
	pxenv->BC_DataSeg = 0;
	pxenv->BC_DataSize = 0;
	pxenv->UNDIDataSeg = SEGMENT(pxe_stack);
	pxenv->UNDIDataSize = end - (void*)pxe_stack;
	pxenv->UNDICodeSeg = SEGMENT(pxe_stack);
	pxenv->UNDICodeSize = end - (void*)pxe_stack;
	pxenv->PXEPtr.segment = SEGMENT(pxe);
	pxenv->PXEPtr.offset = OFFSET(pxe);
	pxenv->Checksum -= byte_checksum ( pxenv, sizeof(*pxenv) );

	/* Install PXE and RM callback code */
	memcpy ( pxe_callback_code, &pxe_callback_interface,
		 pxe_callback_interface_size );
	install_rm_callback_interface ( rm_callback_code, 0 );

	return pxe_stack;
}

/* remove_pxe_stack(): remove PXE stack installed by install_pxe_stack()
 */
void remove_pxe_stack ( void ) {
	forget_base_memory ( pxe_stack, pxe_stack_size() );
	pxe_stack = NULL;
}

#define XXX_REAL_MODE_SS (0x9000);

extern GDT_STRUCT_t(6) _gdt;

/* xstartpxe(): start up a PXE image
 */
int xstartpxe ( void ) {
	int nbp_exit;
	regs_t registers;
	seg_regs_t seg_regs;
	seg_regs_t rm_seg_regs;
	struct {
		segoff_t pxe;
	} PACKED stack_params;
	
	/* Set up registers and stack parameters to pass to PXE NBP */
	seg_regs.cs = 0x28;
	seg_regs.ss = 0x30;
	seg_regs.ds = 0x30;
	seg_regs.es = 0x30;
	seg_regs.fs = 0x30;
	seg_regs.gs = 0x30;
	rm_seg_regs.cs = XXX_REAL_MODE_SS;
	rm_seg_regs.ss = XXX_REAL_MODE_SS;
	rm_seg_regs.ds = PXE_LOAD_SEGMENT;
	rm_seg_regs.es = SEGMENT(&(pxe_stack->pxenv));
	rm_seg_regs.fs = SEGMENT(&(pxe_stack->pxe));
	rm_seg_regs.gs = PXE_LOAD_SEGMENT;
	registers.bx = OFFSET(&(pxe_stack->pxenv));
	registers.dx = OFFSET(&(pxe_stack->pxe));
	stack_params.pxe.segment = SEGMENT(&(pxe_stack->pxe));
	stack_params.pxe.offset = OFFSET(&(pxe_stack->pxe));

	/* Real-mode trampoline fragment used to jump to PXE NBP
	 */
	BEGIN_RM_FRAGMENT(jump_to_pxe_nbp);
	#define xstr(x) #x	/* Macro hackery needed to stringify       */
	#define str(x) xstr(x)	/* the constants PXE_LOAD_{SEGMENT,OFFSET} */
	__asm__ ( "pushw %fs" );
	__asm__ ( "pushw %dx" );
	__asm__ ( "lcall $" str(PXE_LOAD_SEGMENT) ", $" str(PXE_LOAD_OFFSET) );
	__asm__ ( "addw $4, %sp" );
	END_RM_FRAGMENT(jump_to_pxe_nbp);

	/* Call to PXE image */
	gateA20_unset();
	nbp_exit = ext_call
		( 0,
		  EP_REGISTERS ( &registers ),
		  EP_SEG_REGISTERS ( &seg_regs ),
		  EP_STACK ( &rm_seg_regs ),
		  EP_STACK ( &stack_params ),
		  EP_GDT ( &_gdt, 0x28 ),
		  EP_RELOC_STACK ( ( 0x9000 << 4 ) + 0x1000 ),
		  EP_TRAMPOLINE(_prot_to_real, _prot_to_real_end),
		  EP_TRAMPOLINE(jump_to_pxe_nbp, jump_to_pxe_nbp_end),
		  EP_TRAMPOLINE(_real_to_prot, _real_to_prot_end)
		  );
	gateA20_set();

	return nbp_exit;
}

int pxe_in_call ( in_call_data_t *in_call_data, va_list params ) {
	/* i386 calling conventions; the only two defined by Intel's
	 * PXE spec.
	 *
	 * Assembly code must pass a long containing the PXE version
	 * code (i.e. 0x201 for !PXE, 0x200 for PXENV+) as the first
	 * parameter after the in_call opcode.  This is used to decide
	 * whether to take parameters from the stack (!PXE) or from
	 * registers (PXENV+).
	 */
	uint32_t api_version = va_arg ( params, typeof(api_version) );
	uint16_t opcode;
	segoff_t segoff;
	t_PXENV_ANY *structure;
		
	if ( api_version >= 0x201 ) {
		/* !PXE calling convention */
		pxe_call_params_t pxe_params
			= va_arg ( params, typeof(pxe_params) );
		opcode = pxe_params.opcode;
		segoff = pxe_params.segoff;
	} else {
		/* PXENV+ calling convention */
		opcode = in_call_data->pm->regs.bx;
		segoff.segment = in_call_data->rm->seg_regs.es;
		segoff.offset = in_call_data->pm->regs.di;
	}
	structure = VIRTUAL ( segoff.segment, segoff.offset );
	return pxe_api_call ( opcode, structure );
}

#endif /* EXPORT_PXE */
