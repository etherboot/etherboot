/**************************************************************************
Etherboot -  BOOTP/TFTP Bootstrap Program

Author: Martin Renters
  Date: Dec/93

**************************************************************************/

#include "osdep.h"

/* These could be customised for different languages perhaps */
#define	ASK_PROMPT	"Boot from (N)etwork or from (L)ocal? "
#define	ANS_NETWORK	'N'
#define	ANS_LOCAL	'L'
#ifndef	ANS_DEFAULT	/* in case left out in Makefile */
#define	ANS_DEFAULT	ANS_NETWORK
#endif

#if	!defined(TAGGED_IMAGE) && !defined(AOUT_IMAGE) && !defined(ELF_IMAGE)
#define	TAGGED_IMAGE		/* choose at least one */
#endif

#define ESC		'\033'

/*  Edit this to change the path to hostspecific kernel image
    kernel.<client_ip_address> in RARP boot */
#ifndef	DEFAULT_KERNELPATH
#define	DEFAULT_KERNELPATH	"/tftpboot/kernel.%@"
#endif

/* Uncomment this and maybe edit to have a default fallback kernel image.
   This is used if bootp/dhcp-server doesn't provide the kernel path */
/*#define DEFAULT_BOOTFILE	"/tftpboot/kernel"*/

#ifdef FREEBSD_PXEEMU
#undef DEFAULT_BOOTFILE
#ifndef PXENFSROOTPATH
#define PXENFSROOTPATH ""
#endif
#define DEFAULT_BOOTFILE	PXENFSROOTPATH "/boot/pxeboot"
#endif

/* Clean up console settings... mainly CONSOLE_CRT and CONSOLE_SERIAL are used
 * in the sources (except start.S and serial.S which cannot include
 * etherboot.h).  At least one of the CONSOLE_xxx has to be set, and
 * CONSOLE_DUAL sets both CONSOLE_CRT and CONSOLE_SERIAL.  If none is set,
 * CONSOLE_CRT is assumed.  */
#ifdef	CONSOLE_DUAL
#undef CONSOLE_CRT
#define CONSOLE_CRT
#undef CONSOLE_SERIAL
#define CONSOLE_SERIAL
#endif
#if	defined(CONSOLE_CRT) && defined(CONSOLE_SERIAL)
#undef CONSOLE_DUAL
#define CONSOLE_DUAL
#endif
#if	!defined(CONSOLE_CRT) && !defined(CONSOLE_SERIAL)
#define CONSOLE_CRT
#endif

#ifndef	DOWNLOAD_PROTO_NFS
#undef DOWNLOAD_PROTO_TFTP
#define DOWNLOAD_PROTO_TFTP	/* default booting protocol */
#endif

#ifdef	DOWNLOAD_PROTO_TFTP
#define download(fname,loader) tftp((fname),(loader))
#endif
#ifdef	DOWNLOAD_PROTO_NFS
#define download(fname,loader) nfs((fname),(loader))
#endif

#ifndef	MAX_TFTP_RETRIES
#define MAX_TFTP_RETRIES	20
#endif

#ifndef	MAX_BOOTP_RETRIES
#define MAX_BOOTP_RETRIES	20
#endif

#define MAX_BOOTP_EXTLEN	(ETH_MAX_MTU-sizeof(struct bootpip_t))

#ifndef	MAX_ARP_RETRIES
#define MAX_ARP_RETRIES		20
#endif

#ifndef	MAX_RPC_RETRIES
#define MAX_RPC_RETRIES		20
#endif

#define	TICKS_PER_SEC		18

/* Inter-packet retry in ticks */
#define TIMEOUT			(10*TICKS_PER_SEC)

/* Max interval between IGMP packets */
#define IGMP_INTERVAL		(10*TICKS_PER_SEC)

/* These settings have sense only if compiled with -DCONGESTED */
/* total retransmission timeout in ticks */
#define TFTP_TIMEOUT		(30*TICKS_PER_SEC)
/* packet retransmission timeout in ticks */
#define TFTP_REXMT		(3*TICKS_PER_SEC)

#ifndef	NULL
#define NULL	((void *)0)
#endif

/*
   I'm moving towards the defined names in linux/if_ether.h for clarity.
   The confusion between 60/64 and 1514/1518 arose because the NS8390
   counts the 4 byte frame checksum in the incoming packet, but not
   in the outgoing packet. 60/1514 are the correct numbers for most
   if not all of the other NIC controllers.
*/

#define ETH_ALEN		6	/* Size of Ethernet address */
#define ETH_HLEN		14	/* Size of ethernet header */
#define	ETH_ZLEN		60	/* Minimum packet */
#define	ETH_FRAME_LEN		1514	/* Maximum packet */
#ifndef	ETH_MAX_MTU
#define	ETH_MAX_MTU		(ETH_FRAME_LEN-ETH_HLEN)
#endif

#define ARP_CLIENT	0
#define ARP_SERVER	1
#define ARP_GATEWAY	2
#define MAX_ARP		ARP_GATEWAY+1

#define IGMP_SERVER	0
#define MAX_IGMP	IGMP_SERVER+1

#define	RARP_REQUEST	3
#define	RARP_REPLY	4

#define IP		0x0800
#define ARP		0x0806
#define	RARP		0x8035

#define BOOTP_SERVER	67
#define BOOTP_CLIENT	68
#define TFTP_PORT	69
#define SUNRPC_PORT	111

#define IP_ICMP		1
#define IP_IGMP		2
#define IP_UDP		17

/* Same after going through htonl */
#define IP_BROADCAST	0xFFFFFFFF

#define MULTICAST_MASK    0xF0000000
#define MULTICAST_NETWORK 0xE0000000

#define ARP_REQUEST	1
#define ARP_REPLY	2

#define BOOTP_REQUEST	1
#define BOOTP_REPLY	2

#define TAG_LEN(p)		(*((p)+1))
#define RFC1533_COOKIE		99, 130, 83, 99
#define RFC1533_PAD		0
#define RFC1533_NETMASK		1
#define RFC1533_TIMEOFFSET	2
#define RFC1533_GATEWAY		3
#define RFC1533_TIMESERVER	4
#define RFC1533_IEN116NS	5
#define RFC1533_DNS		6
#define RFC1533_LOGSERVER	7
#define RFC1533_COOKIESERVER	8
#define RFC1533_LPRSERVER	9
#define RFC1533_IMPRESSSERVER	10
#define RFC1533_RESOURCESERVER	11
#define RFC1533_HOSTNAME	12
#define RFC1533_BOOTFILESIZE	13
#define RFC1533_MERITDUMPFILE	14
#define RFC1533_DOMAINNAME	15
#define RFC1533_SWAPSERVER	16
#define RFC1533_ROOTPATH	17
#define RFC1533_EXTENSIONPATH	18
#define RFC1533_IPFORWARDING	19
#define RFC1533_IPSOURCEROUTING	20
#define RFC1533_IPPOLICYFILTER	21
#define RFC1533_IPMAXREASSEMBLY	22
#define RFC1533_IPTTL		23
#define RFC1533_IPMTU		24
#define RFC1533_IPMTUPLATEAU	25
#define RFC1533_INTMTU		26
#define RFC1533_INTLOCALSUBNETS	27
#define RFC1533_INTBROADCAST	28
#define RFC1533_INTICMPDISCOVER	29
#define RFC1533_INTICMPRESPOND	30
#define RFC1533_INTROUTEDISCOVER 31
#define RFC1533_INTROUTESOLICIT	32
#define RFC1533_INTSTATICROUTES	33
#define RFC1533_LLTRAILERENCAP	34
#define RFC1533_LLARPCACHETMO	35
#define RFC1533_LLETHERNETENCAP	36
#define RFC1533_TCPTTL		37
#define RFC1533_TCPKEEPALIVETMO	38
#define RFC1533_TCPKEEPALIVEGB	39
#define RFC1533_NISDOMAIN	40
#define RFC1533_NISSERVER	41
#define RFC1533_NTPSERVER	42
#define RFC1533_VENDOR		43
#define RFC1533_NBNS		44
#define RFC1533_NBDD		45
#define RFC1533_NBNT		46
#define RFC1533_NBSCOPE		47
#define RFC1533_XFS		48
#define RFC1533_XDM		49
#ifndef	NO_DHCP_SUPPORT
#define RFC2132_REQ_ADDR	50
#define RFC2132_MSG_TYPE	53
#define RFC2132_SRV_ID		54
#define RFC2132_PARAM_LIST	55
#define RFC2132_MAX_SIZE	57
#define	RFC2132_VENDOR_CLASS_ID	60

#define DHCPDISCOVER		1
#define DHCPOFFER		2
#define DHCPREQUEST		3
#define DHCPACK			5
#endif	/* NO_DHCP_SUPPORT */

#define RFC1533_VENDOR_MAJOR	0
#define RFC1533_VENDOR_MINOR	0

#define RFC1533_VENDOR_MAGIC	128
#define RFC1533_VENDOR_ADDPARM	129
#define	RFC1533_VENDOR_ETHDEV	130
#ifdef	IMAGE_FREEBSD
#define RFC1533_VENDOR_HOWTO    132
#define RFC1533_VENDOR_KERNEL_ENV    133
#endif
#define RFC1533_VENDOR_ETHERBOOT_ENCAP 150
#define RFC1533_VENDOR_MNUOPTS	160
#define RFC1533_VENDOR_NIC_DEV_ID 175
#define RFC1533_VENDOR_SELECTION 176
#define RFC1533_VENDOR_MOTD	184
#define RFC1533_VENDOR_NUMOFMOTD 8
#define RFC1533_VENDOR_IMG	192
#define RFC1533_VENDOR_NUMOFIMG	16

#define RFC1533_END		255

#define BOOTP_VENDOR_LEN	64
#ifndef	NO_DHCP_SUPPORT
#define DHCP_OPT_LEN		312
#endif	/* NO_DHCP_SUPPORT */

#define	TFTP_DEFAULTSIZE_PACKET	512
#define	TFTP_MAX_PACKET		1432 /* 512 */

#define TFTP_RRQ	1
#define TFTP_WRQ	2
#define TFTP_DATA	3
#define TFTP_ACK	4
#define TFTP_ERROR	5
#define TFTP_OACK	6

#define TFTP_CODE_EOF	1
#define TFTP_CODE_MORE	2
#define TFTP_CODE_ERROR	3
#define TFTP_CODE_BOOT	4
#define TFTP_CODE_CFG	5

/* Helper macros used to identify when DHCP options are valid/invalid in/outside of encapsulation */
#define NON_ENCAP_OPT in_encapsulated_options == 0 &&
#ifdef ALLOW_ONLY_ENCAPSULATED
#define ENCAP_OPT in_encapsulated_options == 1 &&
#else
#define ENCAP_OPT
#endif

typedef struct {
	uint32_t	s_addr;
} in_addr;

struct arptable_t {
	in_addr ipaddr;
	uint8_t node[6];
};

struct igmptable_t {
	in_addr group;
	unsigned long time;
};
/*
 * A pity sipaddr and tipaddr are not longword aligned or we could use
 * in_addr. No, I don't want to use #pragma packed.
 */
struct arprequest {
	uint16_t hwtype;
	uint16_t protocol;
	uint8_t  hwlen;
	uint8_t  protolen;
	uint16_t opcode;
	uint8_t  shwaddr[6];
	uint8_t  sipaddr[4];
	uint8_t  thwaddr[6];
	uint8_t  tipaddr[4];
};

struct iphdr {
	uint8_t  verhdrlen;
	uint8_t  service;
	uint16_t len;
	uint16_t ident;
	uint16_t frags;
	uint8_t  ttl;
	uint8_t  protocol;
	uint16_t chksum;
	in_addr src;
	in_addr dest;
};

struct udphdr {
	uint16_t src;
	uint16_t dest;
	uint16_t len;
	uint16_t chksum;
};

struct igmp {
	struct iphdr ip;
	uint8_t  type_ver;
	uint8_t  dummy;
	uint16_t chksum;
	in_addr group;
};

#define IGMP_QUERY	0x11
#define IGMP_REPORT	0x21
#define GROUP_ALL_HOSTS 0xe0000001 /* 224.0.0.1 Host byte order */

/* Format of a bootp packet */
struct bootp_t {
	uint8_t  bp_op;
	uint8_t  bp_htype;
	uint8_t  bp_hlen;
	uint8_t  bp_hops;
	uint32_t bp_xid;
	uint16_t bp_secs;
	uint16_t unused;
	in_addr bp_ciaddr;
	in_addr bp_yiaddr;
	in_addr bp_siaddr;
	in_addr bp_giaddr;
	uint8_t  bp_hwaddr[16];
	uint8_t  bp_sname[64];
	char     bp_file[128];
#ifdef	NO_DHCP_SUPPORT
	uint8_t  bp_vend[BOOTP_VENDOR_LEN];
#else
	uint8_t  bp_vend[DHCP_OPT_LEN];
#endif	/* NO_DHCP_SUPPORT */
};

/* Format of a bootp IP packet */
struct bootpip_t
{
	struct iphdr ip;
	struct udphdr udp;
	struct bootp_t bp;
};

/* Format of bootp packet with extensions */
struct bootpd_t {
	struct bootp_t bootp_reply;
	uint8_t bootp_extension[MAX_BOOTP_EXTLEN];
};

#define	KERNEL_BUF	(BOOTP_DATA_ADDR->bootp_reply.bp_file)

struct tftp_t {
	struct iphdr ip;
	struct udphdr udp;
	unsigned short opcode;
	union {
		uint8_t rrq[TFTP_DEFAULTSIZE_PACKET];
		struct {
			uint16_t block;
			uint8_t  download[TFTP_MAX_PACKET];
		} data;
		struct {
			uint16_t block;
		} ack;
		struct {
			uint16_t errcode;
			uint8_t  errmsg[TFTP_DEFAULTSIZE_PACKET];
		} err;
		struct {
			uint8_t  data[TFTP_DEFAULTSIZE_PACKET+2];
		} oack;
	} u;
};

/* define a smaller tftp packet solely for making requests to conserve stack
   512 bytes should be enough */
struct tftpreq_t {
	struct iphdr ip;
	struct udphdr udp;
	uint16_t opcode;
	union {
		uint8_t rrq[512];
		struct {
			uint16_t block;
		} ack;
		struct {
			uint16_t errcode;
			uint8_t  errmsg[512-2];
		} err;
	} u;
};

#define TFTP_MIN_PACKET	(sizeof(struct iphdr) + sizeof(struct udphdr) + 4)

struct rpc_t {
	struct iphdr ip;
	struct udphdr udp;
	union {
		uint8_t  data[300];		/* longest RPC call must fit!!!! */
		struct {
			uint32_t id;
			uint32_t type;
			uint32_t rpcvers;
			uint32_t prog;
			uint32_t vers;
			uint32_t proc;
			uint32_t data[1];
		} call;
		struct {
			uint32_t id;
			uint32_t type;
			uint32_t rstatus;
			uint32_t verifier;
			uint32_t v2;
			uint32_t astatus;
			uint32_t data[1];
		} reply;
	} u;
};

#define PROG_PORTMAP	100000
#define PROG_NFS	100003
#define PROG_MOUNT	100005

#define MSG_CALL	0
#define MSG_REPLY	1

#define PORTMAP_GETPORT	3

#define MOUNT_ADDENTRY	1
#define MOUNT_UMOUNTALL	4

#define NFS_LOOKUP	4
#define NFS_READ	6

#define NFS_FHSIZE	32

#define NFSERR_PERM	1
#define NFSERR_NOENT	2
#define NFSERR_ACCES	13

/* Block size used for NFS read accesses.  A RPC reply packet (including  all
 * headers) must fit within a single Ethernet frame to avoid fragmentation.
 * Chosen to be a power of two, as most NFS servers are optimized for this.  */
#define NFS_READ_SIZE	1024

#define	FLOPPY_BOOT_LOCATION	0x7c00
/* Must match offsets in loader.S */
#define ROM_SEGMENT		0x1fa
#define ROM_LENGTH		0x1fc

#define	ROM_INFO_LOCATION	(FLOPPY_BOOT_LOCATION+ROM_SEGMENT)
/* at end of floppy boot block */

struct rom_info {
	unsigned short	rom_segment;
	unsigned short	rom_length;
};

extern inline int rom_address_ok(struct rom_info *rom, int assigned_rom_segment)
{
	return (assigned_rom_segment < 0xC000
		|| assigned_rom_segment == rom->rom_segment);
}

/* Define a type for use by setjmp and longjmp */
typedef	struct {
	unsigned long	buf[7];
} jmpbuf[1];

/* Define a type for passing info to a loaded program */
struct ebinfo {
	unsigned char	major, minor;	/* Version */
	unsigned short	flags;		/* Bit flags */
};

/***************************************************************************
External prototypes
***************************************************************************/
/* main.c */
extern void rx_qdrain P((void));
extern int tftp P((const char *name, int (*)(unsigned char *, unsigned int, unsigned int, int)));
extern int ip_transmit P((int len, const void *buf));
extern void build_ip_hdr P((unsigned long destip, int ttl, int protocol, 
	int len, const void *buf));
extern void build_udp_hdr P((unsigned long destip, 
	unsigned int srcsock, unsigned int destsock, int ttl,
	int len, const void *buf));
extern int udp_transmit P((unsigned long destip, unsigned int srcsock,
	unsigned int destsock, int len, const void *buf));
extern int await_reply P((int (*reply)(int ival, void *ptr,
		unsigned short ptype, struct iphdr *ip, struct udphdr *udp),
	int ival, void *ptr, int timeout));
extern int decode_rfc1533 P((unsigned char *, unsigned int, unsigned int, int));
#define RAND_MAX 2147483647L
extern unsigned short ipchksum P((uint16_t *ip, int len));
extern long random P((void));
extern long rfc2131_sleep_interval P((int base, int exp));
extern long rfc1112_sleep_interval P((int base, int exp));
extern void cleanup P((void));

/* nfs.c */
extern void rpc_init(void);
extern int nfs P((const char *name, int (*)(unsigned char *, int, int, int)));
extern void nfs_umountall P((int));

/* proto_slam.c */
extern int url_slam P((const char *name, int (*fnc)(unsigned char *, unsigned int, unsigned int, int)));

/* config.c */
extern void print_config(void);
extern void eth_reset(void);
extern int eth_probe(int adapter);
extern int eth_poll(void);
extern void eth_transmit(const char *d, unsigned int t, unsigned int s, const void *p);
extern void eth_disable(void);

/* osloader.c */
extern int bios_disk_dev;
typedef int (*os_download_t)(unsigned char *data, unsigned int len, int eof);
extern int load_block P((unsigned char *, unsigned int, unsigned int, int ));
extern os_download_t pxe_probe P((unsigned char *data, unsigned int len));

/* misc.c */
extern void twiddle P((void));
extern void sleep P((int secs));
extern void interruptible_sleep P((int secs));
extern int strcasecmp P((const char *a, const char *b));
extern char *substr P((const char *a, const char *b));
extern int getdec P((const char **));
extern void printf P((const char *, ...));
extern int sprintf P((char *, const char *, ...));
extern int inet_aton P((const char *p, in_addr *i));
#ifdef PCBIOS
extern void gateA20_set P((void));
extern void gateA20_unset P((void));
#else
#define gateA20_set()
#define gateA20_unset()
#endif
extern void putchar P((int));
extern int getchar P((void));
extern int iskey P((void));

/* start*.S */
extern int console_getc P((void));
extern void console_putc P((int));
extern int console_ischar P((void));
extern int getshift P((void));
extern int int15 P((int));
#ifdef	POWERSAVE
extern void cpu_nap P((void));
#endif	/* POWERSAVE */
struct e820entry {
	unsigned long long addr;
	unsigned long long size;
	unsigned long type;
#define E820_RAM	1
#define E820_RESERVED	2
#define E820_ACPI	3 /* usable as RAM once ACPI tables have been read */
#define E820_NVS	4
};
#define E820MAX 32
struct meminfo {
	unsigned short basememsize;
	unsigned int memsize;
	int map_count;
	struct e820entry map[E820MAX];
};
extern struct meminfo meminfo;
extern void get_memsizes(void);

extern void disk_init P((void));
extern unsigned int disk_read P((int drv,int c,int h,int s,char *buf));
extern void xstart P((unsigned long, unsigned long, char *));
#ifdef	IMAGE_MULTIBOOT
extern void xend P((void));
#endif
extern unsigned long currticks P((void));
extern int setjmp P((jmpbuf env));
extern void longjmp P((jmpbuf env, int val));
extern void exit P((int status));

/* serial.S */
extern int serial_getc P((void));
extern void serial_putc P((int));
extern int serial_ischar P((void));
extern int serial_init P((void));

/* floppy.c */
extern int bootdisk P((int dev,int part));

/***************************************************************************
External variables
***************************************************************************/
/* main.c */
extern struct rom_info rom;
extern char *hostname;
extern int hostnamelen;
extern jmpbuf restart_etherboot;
extern struct arptable_t arptable[MAX_ARP];
extern struct igmptable_t igmptable[MAX_IGMP];
#ifdef	IMAGE_MENU
extern int menutmo,menudefault;
extern unsigned char *defparams;
extern int defparams_max;
#endif
#ifdef	MOTD
extern unsigned char *motd[RFC1533_VENDOR_NUMOFMOTD];
#endif
extern struct bootpd_t bootp_data;
#define	BOOTP_DATA_ADDR	(&bootp_data)
extern unsigned char *end_of_rfc1533;
#ifdef	IMAGE_FREEBSD
extern int freebsd_howto;
extern char freebsd_kernel_env[];
#endif

/* config.c */
extern struct nic nic;

/* bootmenu.c */

/* osloader.c */

/* created by linker */
extern char _start[], edata[], end[];

/*
 * Local variables:
 *  c-basic-offset: 8
 * End:
 */
