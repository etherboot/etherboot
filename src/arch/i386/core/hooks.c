#include "etherboot.h"

void arch_main ( struct Elf_Bhdr *bhdr __unused ) {

#ifdef PCBIOS
	/* Deallocate memory used for the decompressor, if applicable
	 */
	forget_decompressor_base_memory();
#endif

}
