/* Real-mode interface: C portions.
 *
 * Initial version by Michael Brown <mbrown@fensystems.co.uk>, January 2004.
 */

#include "etherboot.h"
#include "realmode.h"
#include "segoff.h"

#define RM_STACK_SIZE ( 0x1000 )

int _real_call ( void *fragment, int fragment_len,
		 void *in_stack, int in_stack_len,
		 void *out_stack, int out_stack_len ) {
	int trampoline_len = _prot_to_real_prefix_size + fragment_len +
		_real_to_prot_suffix_size;
	char *stack_base;
	char *stack;
	prot_to_real_params_t *p2r_params;
	real_to_prot_params_t *r2p_params;
	int retval;
	/* local_stack must be the last local variable declared */
	char local_stack[in_stack_len + trampoline_len];

	/* Work out where we're putting the stack */
	stack_base = ( virt_to_phys(local_stack) < 0xa0000 ) ? local_stack :
		phys_to_virt(real_mode_stack) - sizeof(local_stack);
	stack = stack_base;

	/* Compile input stack and trampoline code to stack */
	if ( in_stack_len ) {
		memcpy ( stack, in_stack, in_stack_len );
		stack += in_stack_len;
	}
	memcpy ( stack, _prot_to_real_prefix, _prot_to_real_prefix_size );
	stack += _prot_to_real_prefix_size;
	p2r_params = (prot_to_real_params_t*) ( stack - sizeof(*p2r_params) );
	memcpy ( stack, fragment, fragment_len );
	stack += fragment_len;
	memcpy ( stack, _real_to_prot_suffix, _real_to_prot_suffix_size );
	stack += _real_to_prot_suffix_size;
	r2p_params = (real_to_prot_params_t*) ( stack - sizeof(*r2p_params) );

	/* Set parameters within compiled stack */
	p2r_params->ss = p2r_params->cs = SEGMENT ( stack - RM_STACK_SIZE );
	p2r_params->esp = virt_to_phys ( stack_base );
	p2r_params->r2p_params = virt_to_phys ( r2p_params );
	r2p_params->out_stack = ( out_stack == NULL ) ?
		0 : virt_to_phys ( out_stack );
	r2p_params->out_stack_len = out_stack_len;

	/* Switch to physical addresss, call to trampoline, switch
	 * back.  "volatile" is needed to prevent the compiler
	 * switching the order of __asm__() and "let local_stack go
	 * out of scope".
	 */
	__asm__ volatile ( "call _virt_to_phys\n\t"
			   "movl %1, %%eax\n\t"
			   "call *%%eax\n\t"
			   "pushl %%eax\n\t"
			   "call _phys_to_virt\n\t"
			   "popl %0\n\t"
			   : "=r" ( retval )
			   : "r" ( virt_to_phys ( stack_base + in_stack_len ) )
			   : "eax", "ecx", "edx" );

	return retval;
}
