#ifndef ETHERBOOT_I386_HOOKS_H
#define ETHERBOOT_I386_HOOKS_H

static inline void arch_main ( void *ptr __unused ) {}
#define arch_on_exit(status)
#define arch_relocate_to(addr)

#endif /* ETHERBOOT_I386_HOOKS_H */
