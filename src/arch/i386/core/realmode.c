/* Real-mode interface: C portions.
 *
 * Initial version by Michael Brown <mbrown@fensystems.co.uk>, January 2004.
 */

#include "etherboot.h"
#include "realmode.h"
#include "segoff.h"

#define RM_STACK_SIZE ( 0x1000 )

/* Make a call to a real-mode code block.
 */

/* These is the structure that exists on the stack as the paramters
 * passed in to _real_call.  We pass a pointer to this struct to
 * prepare_real_call(), to save stack space.
 */
typedef struct {
	void *fragment;
	int fragment_len;
	void *in_stack;
	int in_stack_len;
	void *out_stack;
	int out_stack_len;
} real_call_params_t;

uint32_t prepare_real_call ( real_call_params_t *p,
			     int local_stack_len, char *local_stack ) {
	char *stack_base;
	char *stack;
	prot_to_real_params_t *p2r_params;
	real_to_prot_params_t *r2p_params;
	
	/* Work out where we're putting the stack */
	stack_base = ( virt_to_phys(local_stack) < 0xa0000 ) ? local_stack :
		phys_to_virt(real_mode_stack) - local_stack_len;
	stack = stack_base;

	/* Compile input stack and trampoline code to stack */
	if ( p->in_stack_len ) {
		memcpy ( stack, p->in_stack, p->in_stack_len );
		stack += p->in_stack_len;
	}
	memcpy ( stack, _prot_to_real_prefix, _prot_to_real_prefix_size );
	stack += _prot_to_real_prefix_size;
	p2r_params = (prot_to_real_params_t*) ( stack - sizeof(*p2r_params) );
	memcpy ( stack, p->fragment, p->fragment_len );
	stack += p->fragment_len;
	memcpy ( stack, _real_to_prot_suffix, _real_to_prot_suffix_size );
	stack += _real_to_prot_suffix_size;
	r2p_params = (real_to_prot_params_t*) ( stack - sizeof(*r2p_params) );

	/* Set parameters within compiled stack */
	p2r_params->ss = p2r_params->cs = SEGMENT ( stack - RM_STACK_SIZE );
	p2r_params->esp = virt_to_phys ( stack_base );
	p2r_params->r2p_params = virt_to_phys ( r2p_params );
	r2p_params->out_stack = ( p->out_stack == NULL ) ?
		0 : virt_to_phys ( p->out_stack );
	r2p_params->out_stack_len = p->out_stack_len;

	return virt_to_phys ( stack_base + p->in_stack_len );
}


/* Parameters are not genuinely unused; they are passed to
 * prepare_real_call() as part of a real_call_params_t struct.
 */
uint16_t _real_call ( void *fragment, int fragment_len,
		      void *in_stack __unused, int in_stack_len,
		      void *out_stack __unused, int out_stack_len __unused ) {
	uint16_t retval;

	/* This code is basically equivalent to
	 *
	 *	uint32_t trampoline;
	 *	char local_stack[ in_stack_len + _prot_to_real_prefix_size +
	 *			  fragment_len + _real_to_prot_suffix_size ];
	 *	trampoline = prepare_real_call ( &fragment, local_stack );
	 *	__asm__ ( "call _virt_to_phys\n\t"
	 *		  "call %%eax\n\t"
	 *		  "call _phys_to_virt\n\t"
	 *		  : "=a" (retval) : "0" (trampoline) );
	 *
	 * but implemented in assembly to avoid problems with not
	 * being certain exactly how gcc handles %esp.
	 */

	__asm__ ( "pushl %%ebp\n\t"
		  "movl  %%esp, %%ebp\n\t"	/* %esp preserved via %ebp */
		  "subl  %%ecx, %%esp\n\t"	/* space for inline RM stack */
		  "pushl %%esp\n\t"		/* set up RM stack */
		  "pushl %%ecx\n\t"
		  "pushl %%eax\n\t"
		  "call  prepare_real_call\n\t"	/* %eax = trampoline addr */
		  "addl  $12, %%esp\n\t"
		  "call  _virt_to_phys\n\t"	/* switch to phys addr */
		  "call  *%%eax\n\t"		/* call to trampoline */
		  "call  _phys_to_virt\n\t"	/* switch to virt addr */
		  "movl  %%ebp, %%esp\n\t"	/* restore %esp & %ebp */
		  "popl  %%ebp\n\t"
		  : "=a" ( retval )
		  : "0" ( &fragment )
		  , "c" ( ( ( in_stack_len + _prot_to_real_prefix_size +
			      fragment_len + _real_to_prot_suffix_size )
			    + 0x3 ) & ~0x3 ) );
	return retval;
}
