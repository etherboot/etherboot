#include "etherboot.h"

void arch_main ( struct Elf_Bhdr *bhdr __unused ) {

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
