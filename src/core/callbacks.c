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
int _ext_call ( void *address, ... ) {
	int ret;
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
int v_ext_call ( void *address, va_list ap ) {
	va_list aq;
	uint32_t first_arg;
	int valid = 1;
	
	va_copy ( aq, ap );
	first_arg = va_arg ( aq, typeof(first_arg) );
	if ( first_arg == EP_TRACE ) {
		valid = v_ext_call_check ( address, aq );
		/* Pop first_arg from ap as well */
		va_arg ( ap, typeof(first_arg) );
	}
	va_end ( aq );
	if ( !valid ) {
		printf ( "*** Refusing to perform external call ***\n" );
		return 0xffffffff;
	}
	return _v_ext_call ( address, ap );
}

/* Print out parameter list for a call to ext_call().
 */
int v_ext_call_check ( void *address, va_list ap ) {
	int type;

	printf ( "External call to %x with arguments:\n", address );
	/* Dump argument list */
	while ( ( type = va_arg ( ap, typeof(type) ) ) != EXTCALL_END_LIST ) {
		if ( ! v_ec_check_param ( type, &ap ) ) {
			printf ( " Unknown argument type %hx\n", type );
			return 0;
		}
	}
	printf ( "End of argument list.  Breakpoint at physical %x\n",
		 virt_to_phys(extcall_breakpoint) );
	return 1;
}

int v_ec_check_param ( int type, va_list *ap ) {
	
	switch ( type ) {
	case EXTCALL_NONE: {
		printf ( " Null argument\n" );
	} break;
	case EXTCALL_STACK: {
		char *stack_base;
		int stack_length;
		int i;
		stack_base = va_arg ( *ap, typeof(stack_base) );
		stack_length = va_arg ( *ap, typeof(stack_length) );
		printf ( " Stack parameters at %x length %hx:\n  ",
			 stack_base, stack_length );
		for ( i = 0; i < stack_length; i++ ) {
			printf ( "%hhx ", stack_base[i] );
		}
		putchar ( '\n' );
	} break;
	case EXTCALL_RET_STACK: {
		void *ret_stack_base;
		int ret_stack_max;
		int *ret_stack_len;
		ret_stack_base = va_arg ( *ap, typeof(ret_stack_base) );
		ret_stack_max = va_arg ( *ap, typeof(ret_stack_max) );
		ret_stack_len = va_arg ( *ap, typeof(ret_stack_len) );
		printf ( " Return stack at %x length %hx",
			 ret_stack_base, ret_stack_max );
		if ( ret_stack_len != NULL ) {
			printf ( " return length stored at %x",
				 ret_stack_len );
		}
		putchar ( '\n' );
	} break;
	case EXTCALL_RELOC_STACK: {
		void *reloc_stack;
		reloc_stack = va_arg ( *ap, typeof(reloc_stack) );
		printf ( " Relocate stack to [......,%x)\n",
			 reloc_stack );
	} break;
	case EXTCALL_TRAMPOLINE: {
		void *routine;
		int length;
		routine = va_arg ( *ap, typeof(routine) );
		length = va_arg ( *ap, typeof(length) );
		printf ( " Trampoline routine at %x length %hx\n",
			 routine, length );
	} break;
	default:
		/* Hand off to arch-specific interpreter */
		return v_arch_ec_check_param ( type, ap );
		
	}
	return 1;
}

#endif /* DEBUG_EXT_CALL */
