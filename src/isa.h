#if !defined(ISA_H) && defined(CONFIG_ISA)
#define ISA_H

struct dev;

struct isa_driver
{
	int type;
	const char *name;
	int (*probe)(struct dev *, unsigned short *);
	unsigned short *ioaddrs;
};

#define __isa_driver	__attribute__ ((unused,__section__(".rodata.isa_drivers")))
extern const struct isa_driver isa_drivers[];
extern const struct isa_driver eisa_drivers[];

#endif /* ISA_H */

