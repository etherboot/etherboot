#include "etherboot.h"
#include "callbacks.h"
#include <stdarg.h>

void arch_main ( in_call_data_t *data, va_list params __unused ) {

#ifdef PCBIOS
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

void arch_on_exit ( int exit_status __unused ) {
	/* Deallocate the real-mode stack now.  We will probably use
	 * it after this point, but we need to free up the memory and
	 * we probably aren't going to allocate any more after this,
	 * so it should be safe.
	 */
	forget_real_mode_stack();
}
