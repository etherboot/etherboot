struct multiboot_mods {
	unsigned mod_start;
	unsigned mod_end;
	unsigned char *string;
	unsigned reserved;
};


/* The structure of a Multiboot 0.6 parameter block.  */
struct multiboot_info {
	unsigned int flags;
#define MULTIBOOT_MEM_VALID       0x01
#define MULTIBOOT_BOOT_DEV_VALID  0x02
#define MULTIBOOT_CMDLINE_VALID   0x04
#define MULTIBOOT_MODS_VALID      0x08
#define MULTIBOOT_AOUT_SYMS_VALID 0x10
#define MULTIBOOT_ELF_SYMS_VALID  0x20
#define MULTIBOOT_MMAP_VALID      0x40
	unsigned int memlower;
	unsigned int memupper;
	unsigned int bootdev;
	unsigned int cmdline;	/* physical address of the command line */
	unsigned mods_count;
	struct multiboot_mods *mods_addr;
	unsigned syms_num;
	unsigned syms_size;
	unsigned syms_addr;
	unsigned syms_shndx;
	unsigned mmap_length;
	struct e820entry *mmap_addr;
	/* The structure actually ends here, so I might as well put
	 * the ugly e820 parameters here...
	 */
	unsigned e820entry_size;
	struct e820entry mmap[E820MAX];
};

static struct multiboot_info mbinfo;

static inline void multiboot_boot(unsigned long entry)
{
	unsigned char cmdline[512], *c;
	int i;
	/* Etherboot limits the command line to the kernel name,
	 * default parameters and user prompted parameters.  All of
	 * them are shorter than 256 bytes.  As the kernel name and
	 * the default parameters come from the same BOOTP/DHCP entry
	 * (or if they don't, the parameters are empty), only two
	 * strings of the maximum size are possible.  Note this buffer
	 * can overrun if a stupid file name is chosen.  Oh well.  */
	c = cmdline;
	for (i = 0; KERNEL_BUF[i] != 0; i++) {
		switch (KERNEL_BUF[i]) {
		case ' ':
		case '\\':
		case '"':
			*c++ = '\\';
			break;
		default:
		}
		*c++ = KERNEL_BUF[i];
	}
	(void)sprintf(c, " -retaddr %#lX", virt_to_phys(xend32));

	mbinfo.flags = MULTIBOOT_MMAP_VALID | MULTIBOOT_MEM_VALID |MULTIBOOT_CMDLINE_VALID;
	mbinfo.memlower = meminfo.basememsize;
	mbinfo.memupper = meminfo.memsize;
	mbinfo.bootdev = 0;	/* not booted from disk */
	mbinfo.cmdline = virt_to_phys(cmdline);
	mbinfo.e820entry_size = sizeof(struct e820entry);
	mbinfo.mmap_length = 
		mbinfo.e820entry_size * meminfo.map_count;
	mbinfo.mmap_addr = mbinfo.mmap;
	memcpy(mbinfo.mmap, meminfo.map, mbinfo.mmap_length);
	
	/* The Multiboot 0.6 spec requires all segment registers to be
	 * loaded with an unrestricted, writeable segment.
	 * xstart32 does this for us.
	 */
	
	/* Start the kernel, passing the Multiboot information record
	 * and the magic number.  */
	os_regs.eax = 0x2BADB002;
	os_regs.ebx = virt_to_phys(&mbinfo);
	xstart32(entry);
	longjmp(restart_etherboot, -2);
}
