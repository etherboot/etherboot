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
#include "nic.h"
#include "disk.h"

jmpbuf			restart_etherboot;

#ifdef	IMAGE_FREEBSD
int freebsd_howto = 0;
char freebsd_kernel_env[256];
#endif

#ifdef FREEBSD_PXEEMU
extern char		pxeemu_nbp_active;
#endif	/* FREEBSD_PXEBOOT */

static inline void ask_boot(void)
{
#if defined(ASK_BOOT) && ASK_BOOT > 0
	while(1) {
		int c;
		unsigned long time;
		printf(ASK_PROMPT);
		for (time = currticks() + ASK_BOOT*TICKS_PER_SEC; !iskey(); )
			if (currticks() > time) {
				c = ANS_DEFAULT;
				goto done;
			}
		c = getchar();
		if ((c >= 'a') && (c <= 'z')) c &= 0x5F;
		if (c == '\n') c = ANS_DEFAULT;
done:
		if ((c >= ' ') && (c <= '~')) putchar(c);
		putchar('\n');
		if (c == ANS_LOCAL)
			exit(0);
		if (c == ANS_NETWORK)
			break;
	}
#endif /* ASK_BOOT */
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

static int classes[] = { NIC_DRIVER, DISK_DRIVER, FLOPPY_DRIVER };

/**************************************************************************
MAIN - Kick off routine
**************************************************************************/
int main(void)
{
	char *p;
	static int card_retries = 0;
	int type;
	struct dev *dev;
	struct class_operations *ops;
	int state, i;
	void *heap_base;

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
	cleanup();
	relocate();
	console_init();
	init_heap();
	/* -1:	timeout or ESC
	   -2:	error return from loader
	   0:	retry booting bootp and tftp
	   1:   retry tftp with possibly modified bootp reply
	   2:   retry bootp and tftp
	   3:   retry probe bootp and tftp
	   255: exit Etherboot
	*/
	state = setjmp(restart_etherboot);
	if ((state >= 1) && (state <= 2)) {
		dev->how_probe = PROBE_AWAKE;
		if ((dev->how_probe = ops->probe(dev)) == PROBE_FAILED) {
			state = -1;
		}
		
	}
	while(1) {
		switch(state) {
		case 0:
			/* First time through */
			heap_base = allot(0);
			i = -1;
			state = 4;

			/* We just called setjmp ... */
			ask_boot();
			try_floppy_first();
			break;
		case 1:
			/* Any return from load is a failure */
			ops->load(dev);
			state = -1;
			break;
		case 2:
			state = -1;
			if (ops->load_configuration(dev) >= 0) {
				state = 1;
			}
			break;
		case 3:
			state = -1;
			heap_base = allot(0);
#ifdef MULTICAST_LEVEL2
			memset(&igmptable, 0, sizeof(igmptable));
#endif
			dev->how_probe = ops->probe(dev);
			if (dev->how_probe == PROBE_FAILED) {
				state = 4;
			} else {
				state = 2;
			}
			break;
		case 4:
			cleanup();
			forget(heap_base);
			i += 1;
			if (i > sizeof(classes)/sizeof(classes[0])) {
				printf("No adapter found\n");
				interruptible_sleep(++card_retries);
				state = 0;
				break;
			}
			type = classes[i];
			ops = &operations[type];
			dev = ops->dev;
			dev->how_probe = PROBE_FIRST;
			dev->type = type;
			state = 3;
				break;
		case 255:
			exit(0);
			state = -1;
			break;
		default:
			printf("<abort>\n");
#ifdef EMERGENCYDISKBOOT
			exit(0);
#endif
			interruptible_sleep(2);
			state = 4;
			break;
		}
	}
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
};

int loadkernel(const char *fname)
{
	static const struct proto * const last_proto = 
		&protos[sizeof(protos)/sizeof(protos[0])];
	const struct proto *proto;
	in_addr ip;
	int len;
	const char *name;
	int port;

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
	port = -1;
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
				port = strtoul(name, &name, 10);
			}
		}
		if (name[0] == '/') {
			/* FIXME where do I pass the port? */
			arptable[ARP_SERVER].ipaddr.s_addr = ip.s_addr;
			return proto->load(name + 1, load_block);
		}
	}
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
