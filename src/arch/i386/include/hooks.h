#ifndef ETHERBOOT_I386_HOOKS_H
#define ETHERBOOT_I386_HOOKS_H

struct Elf_Bhdr;
void arch_main ( struct Elf_Bhdr * );
#define arch_on_exit(status)
#define arch_relocate_to(addr)
void arch_relocated_from ( uint32_t old_addr );

#endif /* ETHERBOOT_I386_HOOKS_H */
