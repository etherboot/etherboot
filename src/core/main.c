/**************************************************************************
Etherboot -  BOOTP/TFTP Bootstrap Program

Author: Martin Renters
  Date: Dec/93

Literature dealing with the network protocols:
	ARP - RFC826
	RARP - RFC903
	UDP - RFC768
	BOOTP - RFC951, RFC2132 (vendor extensions)
	DHCP - RFC2131, RFC2132 (options)
	TFTP - RFC1350, RFC2347 (options), RFC2348 (blocksize), RFC2349 (tsize)
	RPC - RFC1831, RFC1832 (XDR), RFC1833 (rpcbind/portmapper)
	NFS - RFC1094, RFC1813 (v3, useful for clarifications, not implemented)
	IGMP - RFC1112

**************************************************************************/

/* #define MDEBUG */

#include "etherboot.h"
#include "dev.h"
#include "nic.h"
#include "disk.h"
#include "timer.h"

jmp_buf	restart_etherboot;
int	url_port;		

#ifdef	IMAGE_FREEBSD
int freebsd_howto = 0;
char freebsd_kernel_env[FREEBSD_KERNEL_ENV_SIZE];
#endif

#ifdef FREEBSD_PXEEMU
extern char		pxeemu_nbp_active;
#endif	/* FREEBSD_PXEBOOT */

static inline unsigned long ask_boot(void)
{
	unsigned long order = DEFAULT_BOOT_ORDER;
#ifdef LINUXBIOS
	order = get_boot_order(order);
#endif
#if defined(ASK_BOOT) && ASK_BOOT > 0
	while(1) {
		int c;
		unsigned long time;
		printf(ASK_PROMPT);
		for (time = currticks() + ASK_BOOT*TICKS_PER_SEC; !iskey(); ) {
			if (currticks() > time) {
				c = ANS_DEFAULT;
				goto done;
			}
		}
		c = getchar();
		if ((c >= 'a') && (c <= 'z')) c &= 0x5F;
		if ((c >= ' ') && (c <= '~')) putchar(c);
		putchar('\n');
done:
		switch(c) {
		default:
			/* Nothing useful try again */
			continue;
		case ANS_LOCAL:
			order = BOOT_NOTHING;
			break;
		case ANS_DEFAULT:
			/* Preserve the default boot order */
			break;
		case ANS_NETWORK:
			order = (BOOT_NIC     << (0*BOOT_BITS)) | 
				(BOOT_NOTHING << (1*BOOT_BITS));
			break;
		case ANS_DISK:
			order = (BOOT_DISK    << (0*BOOT_BITS)) | 
				(BOOT_NOTHING << (1*BOOT_BITS));
			break;
		case ANS_FLOPPY:
			order = (BOOT_FLOPPY  << (0*BOOT_BITS)) | 
				(BOOT_NOTHING << (1*BOOT_BITS));
			break;
		}
		break;
	}
	putchar('\n');
#endif /* ASK_BOOT */
	return order;
}

static inline void try_floppy_first(void)
{
#if (TRY_FLOPPY_FIRST > 0) && defined(CAN_BOOT_DISK)
	int i;
	printf("Trying floppy");
	disk_init();
	for (i = TRY_FLOPPY_FIRST; i-- > 0; ) {
		putchar('.');
		if (pcbios_disk_read(0, 0, 0, 0, ((char *)FLOPPY_BOOT_LOCATION)) != 0x8000) {
			printf("using floppy\n");
			exit(0);
		}
	}
	printf("no floppy\n");
#endif /* TRY_FLOPPY_FIRST */	
}

static void console_init(void)
{
#ifdef	CONSOLE_SERIAL
	(void)serial_init();
#endif
}

static void console_fini(void)
{
#ifdef	CONSOLE_SERIAL
	(void)serial_fini();
#endif
}

static struct class_operations {
	struct dev *dev;
	int (*probe)(struct dev *dev);
	int (*load_configuration)(struct dev *dev);
	int (*load)(struct dev *dev);
}
operations[] = {
	{ &nic.dev,  eth_probe,  eth_load_configuration,  eth_load  },
	{ &disk.dev, disk_probe, disk_load_configuration, disk_load },
	{ &disk.dev, disk_probe, disk_load_configuration, disk_load },
};


static int main_loop(int state);
static int exit_ok;
static int exit_status;

/**************************************************************************
MAIN - Kick off routine
**************************************************************************/
int main(struct Elf_Bhdr *ptr)
{
	char *p;
	int state;

	for (p = _bss; p < _ebss; p++)
		*p = 0;	/* Zero BSS */

	console_init();

#ifdef	DELIMITERLINES
	for (i=0; i<80; i++) putchar('=');
#endif
	rom = *(struct rom_info *)phys_to_virt(ROM_INFO_LOCATION);
	printf("ROM segment %#hx length %#hx reloc %#x\n", rom.rom_segment,
		rom.rom_length << 1, (unsigned long)_text);

	gateA20_set();
	print_config();
	get_memsizes();

	/* -1:	timeout or ESC
	   -2:	error return from loader
	   -3:  finish the current run.
	   0:	retry booting bootp and tftp
	   1:   retry tftp with possibly modified bootp reply
	   2:   retry bootp and tftp
	   3:   retry probe bootp and tftp
	   4:   start with the next device and retry from there...
	   255: exit Etherboot
	   256: retry after relocation
	*/
	state = setjmp(restart_etherboot);
	exit_ok = 1;
	for(;state != 255;) {
		state = main_loop(state);
	}
	return exit_status;
}

void exit(int status)
{
	while(!exit_ok)
		;
	exit_status = status;
	longjmp(restart_etherboot, 255);
}

static int main_loop(int state)
{
	/* Splitting main into 2 pieces makes the semantics of 
	 * which variables are preserved across a longjmp clean
	 * and predictable.
	 */
	static unsigned long order;
	static struct dev * dev = 0;
	static struct class_operations *ops;
	static void *heap_base;
	static int type;
	static int i;

	if (((state >= 1) && (state <= 2)) && dev &&
		(dev->how_probe == PROBE_AWAKE)) {
		if ((dev->how_probe = ops->probe(dev)) == PROBE_FAILED) {
			state = -1;
		}
		
	}
	switch(state) {
	case 0:
	{
		static int firsttime = 1;
		/* First time through */
		if (firsttime) {
			relocate();
			cleanup();
			console_init();
			setup_timers();
			init_heap();

			firsttime = 0;
		} 
#ifdef EMERGENCYDISKBOOT
		else {
			cleanup();
			exit(0);
		}
#endif
		heap_base = allot(0);
		i = -1;
		state = 4;
		dev = 0;

		/* We just called setjmp ... */
		order = ask_boot();
		try_floppy_first();
		break;
	}
	case 4:
		cleanup();
		console_init();
		forget(heap_base);
		/* Find a dev entry to probe with */
		if (!dev) {
			int boot;
			int failsafe;

			/* Advance to the next device type */
			i += 1;
			boot = (order >> (i * BOOT_BITS)) & BOOT_MASK;
			type = boot & BOOT_TYPE_MASK;
			failsafe = (boot & BOOT_FAILSAFE) != 0;
			if (i >= MAX_BOOT_ENTRIES) {
				type = BOOT_NOTHING;
			}
			if ((i == 0) && (type == BOOT_NOTHING)) {
				/* Return to caller */
				exit(0);
			}
			if (type >= BOOT_NOTHING) {
				interruptible_sleep(2);
				state = 0;
				break;
			}
			ops = &operations[type];
			dev = ops->dev;
			dev->how_probe = PROBE_FIRST;
			dev->type = type;
			dev->failsafe = failsafe;
		} else {
			/* Advance to the next device of the same type */
			dev->how_probe = PROBE_NEXT;
		}
		state = 3;
		break;
	case 3:
		state = -1;
		heap_base = allot(0);
		dev->how_probe = ops->probe(dev);
		if (dev->how_probe == PROBE_FAILED) {
			dev = 0;
			state = 4;
		} else {
			state = 2;
		}
		break;
	case 2:
		state = -1;
		if (ops->load_configuration(dev) >= 0) {
			state = 1;
		}
		break;
	case 1:
		/* Any return from load is a failure */
		ops->load(dev);
		state = -1;
		break;
	case 256:
		state = 0;
		break;
	case -3:
		i = MAX_BOOT_ENTRIES;
		type = BOOT_NOTHING;
		/* fall through */
	default:
		printf("<abort>\n");
		state = 4;
		/* At the end goto state 0 */
		if ((type >= BOOT_NOTHING) || (i >= MAX_BOOT_ENTRIES)) {
			state = 0;
		}
		break;
	}
	return state;
}


/**************************************************************************
LOADKERNEL - Try to load kernel image
**************************************************************************/
struct proto {
	char *name;
	int (*load)(const char *name,
		int (*fnc)(unsigned char *, unsigned int, unsigned int, int));
};
static const struct proto protos[] = {
#ifdef DOWNLOAD_PROTO_TFTM
	{ "x-tftm", url_tftm },
#endif
#ifdef DOWNLOAD_PROTO_SLAM
	{ "x-slam", url_slam },
#endif
#ifdef DOWNLOAD_PROTO_NFS
	{ "nfs", nfs },
#endif
#ifdef DOWNLOAD_PROTO_DISK
	{ "file", url_file },
#endif
#ifdef DOWNLOAD_PROTO_TFTP
	{ "tftp", tftp },
#endif
};

int loadkernel(const char *fname)
{
	static const struct proto * const last_proto = 
		&protos[sizeof(protos)/sizeof(protos[0])];
	const struct proto *proto;
	in_addr ip;
	int len;
	const char *name;

#if 0 && defined(CAN_BOOT_DISK)
	if (!memcmp(fname,"/dev/",5) && fname[6] == 'd') {
		int dev, part = 0;
		if (fname[5] == 'f') {
			if ((dev = fname[7] - '0') < 0 || dev > 3)
				goto nodisk; }
		else if (fname[5] == 'h' || fname[5] == 's') {
			if ((dev = 0x80 + fname[7] - 'a') < 0x80 || dev > 0x83)
				goto nodisk;
			if (fname[8]) {
				part = fname[8] - '0';
				if (fname[9])
					part = 10*part + fname[9] - '0'; }
			/* bootdisk cannot cope with more than eight partitions */
			if (part < 0 || part > 8)
				goto nodisk; }
		else
			goto nodisk;
		return(bootdisk(dev, part, load_block));
	}
#endif
	ip.s_addr = arptable[ARP_SERVER].ipaddr.s_addr;
	name = fname;
	url_port = -1;
	len = 0;
	while(fname[len] && fname[len] != ':') {
		len++;
	}
	for(proto = &protos[0]; proto < last_proto; proto++) {
		if (memcmp(name, proto->name, len) == 0) {
			break;
		}
	}
	if ((proto < last_proto) && (memcmp(fname + len, "://", 3) == 0)) {
		name += len + 3;
		if (name[0] != '/') {
			name += inet_aton(name, &ip);
			if (name[0] == ':') {
				name++;
				url_port = strtoul(name, &name, 10);
			}
		}
		if (name[0] == '/') {
			arptable[ARP_SERVER].ipaddr.s_addr = ip.s_addr;
			printf( "Loading %s ", fname );
			return proto->load(name + 1, load_block);
		}
	}
#ifdef	DOWNLOAD_PROTO_TFTP
	printf("Loading %@:%s ", arptable[ARP_SERVER].ipaddr, fname);
#endif
	return tftp(fname, load_block);
}


/**************************************************************************
CLEANUP - shut down networking and console so that the OS may be called 
**************************************************************************/
void cleanup(void)
{
#ifdef	DOWNLOAD_PROTO_NFS
	nfs_umountall(ARP_SERVER);
#endif
	/* Stop receiving packets */
	eth_disable();
	disk_disable();
	console_fini();
}

/*
 * Local variables:
 *  c-basic-offset: 8
 * End:
 */
