#ifndef ETHERBOOT_IA64_HOOKS_H
#define ETHERBOOT_IA64_HOOKS_H

struct Elf_Bhdr;
void arch_main(struct Elf_Bhdr *ptr);
void arch_on_exit(int status);
void arch_relocate_to(unsigned long addr);

#endif /* ETHERBOOT_IA64_HOOKS_H */
