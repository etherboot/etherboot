#include "etherboot.h"
#include "callbacks.h"
#include <stdarg.h>

void arch_main ( in_call_data_t *data, va_list params ) {

#ifdef PCBIOS

	/* If entered via real mode, set up exit interceptor */
	if ( data->rm != NULL ) {
		data->intercept->fnc = exit_via_prefix;
	}

	/* Deallocate base memory used for the decompressor, if applicable
	 */
	forget_decompressor_base_memory();
#endif

}

void arch_relocated_from ( uint32_t old_addr ) {

#ifdef PCBIOS
	/* Deallocate base memory used for the Etherboot runtime,
	 * if applicable
	 */
	forget_runtime_base_memory( old_addr );
#endif

}
