#ifndef ETHERBOOT_I386_HOOKS_H
#define ETHERBOOT_I386_HOOKS_H

struct Elf_Bhdr;
void arch_main ( struct Elf_Bhdr * __unused );
#define arch_on_exit(status)
#define arch_relocate_to(addr)

#endif /* ETHERBOOT_I386_HOOKS_H */
