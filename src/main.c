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

jmpbuf			restart_etherboot;
struct arptable_t	arptable[MAX_ARP];
#if MULTICAST_LEVEL2
struct igmptable_t	igmptable[MAX_IGMP];
#endif
/* Currently no other module uses rom, but it is available */
struct rom_info		rom;
static unsigned long	netmask;
/* Used by nfs.c */
char *hostname = "";
int hostnamelen = 0;
static unsigned long xid;
unsigned char *end_of_rfc1533 = NULL;
static int vendorext_isvalid;
static const unsigned char vendorext_magic[] = {0xE4,0x45,0x74,0x68}; /* äEth */
static const unsigned char broadcast[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
static const in_addr zeroIP = { 0L };

#ifdef	IMAGE_FREEBSD
int freebsd_howto = 0;
char freebsd_kernel_env[256];
#endif

struct bootpd_t bootp_data;

#ifdef	NO_DHCP_SUPPORT
static unsigned char    rfc1533_cookie[5] = { RFC1533_COOKIE, RFC1533_END };
#else	/* !NO_DHCP_SUPPORT */
static int dhcp_reply;
static in_addr dhcp_server = { 0L };
static in_addr dhcp_addr = { 0L };
static unsigned char	rfc1533_cookie[] = { RFC1533_COOKIE };
static const unsigned char dhcpdiscover[] = {
	RFC2132_MSG_TYPE,1,DHCPDISCOVER,
	RFC2132_MAX_SIZE,2,	/* request as much as we can */
	ETH_MAX_MTU / 256, ETH_MAX_MTU % 256,
	RFC2132_VENDOR_CLASS_ID,13,'E','t','h','e','r','b','o','o','t',
	'-',VERSION_MAJOR+'0','.',VERSION_MINOR+'0',
	RFC2132_PARAM_LIST,4,RFC1533_NETMASK,RFC1533_GATEWAY,
	RFC1533_HOSTNAME,RFC1533_VENDOR
};
static const unsigned char dhcprequest [] = {
	RFC2132_MSG_TYPE,1,DHCPREQUEST,
	RFC2132_SRV_ID,4,0,0,0,0,
	RFC2132_REQ_ADDR,4,0,0,0,0,
	RFC2132_MAX_SIZE,2,	/* request as much as we can */
	ETH_MAX_MTU / 256, ETH_MAX_MTU % 256,
	RFC2132_VENDOR_CLASS_ID,13,'E','t','h','e','r','b','o','o','t',
	'-',VERSION_MAJOR+'0','.',VERSION_MINOR+'0',
	/* request parameters */
	RFC2132_PARAM_LIST,
#ifdef	IMAGE_FREEBSD
	/* 5 standard + 7 vendortags + 8 motd + 16 menu items */
	5 + 7 + 8 + 16,
#else
	/* 5 standard + 6 vendortags + 8 motd + 16 menu items */
	5 + 6 + 8 + 16,
#endif
	/* Standard parameters */
	RFC1533_NETMASK, RFC1533_GATEWAY,
	RFC1533_HOSTNAME,RFC1533_VENDOR,
	RFC1533_ROOTPATH,	/* only passed to the booted image */
	/* Etherboot vendortags */
	RFC1533_VENDOR_MAGIC,
	RFC1533_VENDOR_ADDPARM,
	RFC1533_VENDOR_ETHDEV,
	RFC1533_VENDOR_ETHERBOOT_ENCAP,
#ifdef	IMAGE_FREEBSD
	RFC1533_VENDOR_HOWTO,
	/*
	RFC1533_VENDOR_KERNEL_ENV,
	*/
#endif
	RFC1533_VENDOR_MNUOPTS, RFC1533_VENDOR_SELECTION,
	/* 8 MOTD entries */
	RFC1533_VENDOR_MOTD,
	RFC1533_VENDOR_MOTD+1,
	RFC1533_VENDOR_MOTD+2,
	RFC1533_VENDOR_MOTD+3,
	RFC1533_VENDOR_MOTD+4,
	RFC1533_VENDOR_MOTD+5,
	RFC1533_VENDOR_MOTD+6,
	RFC1533_VENDOR_MOTD+7,
	/* 16 image entries */
	RFC1533_VENDOR_IMG,
	RFC1533_VENDOR_IMG+1,
	RFC1533_VENDOR_IMG+2,
	RFC1533_VENDOR_IMG+3,
	RFC1533_VENDOR_IMG+4,
	RFC1533_VENDOR_IMG+5,
	RFC1533_VENDOR_IMG+6,
	RFC1533_VENDOR_IMG+7,
	RFC1533_VENDOR_IMG+8,
	RFC1533_VENDOR_IMG+9,
	RFC1533_VENDOR_IMG+10,
	RFC1533_VENDOR_IMG+11,
	RFC1533_VENDOR_IMG+12,
	RFC1533_VENDOR_IMG+13,
	RFC1533_VENDOR_IMG+14,
	RFC1533_VENDOR_IMG+15,
};

#ifdef	REQUIRE_VCI_ETHERBOOT
int	vci_etherboot;
#endif
#endif	/* NO_DHCP_SUPPORT */

/*
 *	Normally I would arrange the functions in a file to avoid forward
 *	declarations, but in this case I like to see main() as the first
 *	routine.
 */
static int bootp(void);
static int rarp(void);
static void load_configuration(void);
static void load(void);
static unsigned short udpchksum(struct iphdr *packet);
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
		if (disk_read(0, 0, 0, 0, ((char *)FLOPPY_BOOT_LOCATION)) != 0x8000) {
			printf("using floppy\n");
			exit(0);
		}
	}
	printf("no floppy\n");
#endif /* TRY_FLOPPY_FIRST */	
}

/**************************************************************************
MAIN - Kick off routine
**************************************************************************/
int main(void)
{
	char *p;
	static int card_retries = 0;
	static int this_adapter, adapter = -1;
	int i;

	for (p = _bss; p < _ebss; p++)
		*p = 0;	/* Zero BSS */

#ifdef	CONSOLE_SERIAL
	(void)serial_init();
#endif

#ifdef	DELIMITERLINES
	for (i=0; i<80; i++) putchar('=');
#endif

	rom = *(struct rom_info *)ROM_INFO_LOCATION;
	printf("ROM segment %#hx length %#hx reloc %#hx\n", rom.rom_segment,
		rom.rom_length << 1, ((unsigned long)_text) >> 4);

	gateA20_set();
	print_config();
	get_memsizes();
	relocate();
	init_heap();
	/* -1:	timeout or ESC
	   -2:	error return from loader
	   0:	retry booting bootp and tftp
	   1:   retry tftp with possibly modified bootp reply
	   2:   retry bootp and tftp
	   255: exit Etherboot
	*/
	while(1) {
		void *heap_base;
		heap_base = allot(0);
#ifdef MULTICAST_LEVEL2
		memset(&igmptable, 0, sizeof(igmptable));
#endif
		i = setjmp(restart_etherboot);
		if (i == 0) {
			/* We just called setjmp ... */
			ask_boot();
			try_floppy_first();
			this_adapter = adapter;
			if ((adapter = eth_probe(adapter)) < 0) {
				printf("No adapter found\n");
				interruptible_sleep(++card_retries);
				longjmp(restart_etherboot, -1);
			}
			load_configuration();
			load();
		}
		/* We only come to the following functions
		 * through an explicit longjmp
		 */
		else if (i == 1) {
			if ((adapter = eth_probe(this_adapter)) < 0) {
				longjmp(restart_etherboot, -1);
			}
			load();
		}
		else if (i == 2) {
			if ((adapter = eth_probe(this_adapter)) < 0) {
				longjmp(restart_etherboot, -1);
			}
			load_configuration();
			load();
		}
		else if (i == 255) {
			exit(0);
			longjmp(restart_etherboot, -1);
		}
		else {
			printf("<abort>\n");
#ifdef EMERGENCYDISKBOOT
			exit(0);
#endif
			/* Fall through and restart asking for files */
		}
		/* Reset the adapter to it's default state */
		cleanup();
		/* Free everything we have allocated */
		forget(heap_base);
	}
}


/**************************************************************************
LOADKERNEL - Try to load kernel image
**************************************************************************/
#if 1
extern int url_file(const char *name,
	int (*fnc)(unsigned char *, unsigned int, unsigned int, int));

#endif
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

/*
 * Find out what our boot parameters are
 */
static void load_configuration(void)
{
	int server_found;
	/* Find a server to get BOOTP reply from */
#ifdef	RARP_NOT_BOOTP
	printf("Searching for server (RARP)...\n");
#else
#ifndef	NO_DHCP_SUPPORT
	printf("Searching for server (DHCP)...\n");
#else
	printf("Searching for server (BOOTP)...\n");
#endif
#endif

#ifdef	RARP_NOT_BOOTP
	server_found = rarp();
#else
	server_found = bootp();
#endif
	if (!server_found) {
		printf("No Server found\n");
		longjmp(restart_etherboot, -1);
	}
}

/**************************************************************************
LOAD - Try to get booted
**************************************************************************/
static void load(void)
{
	const char	*kernel;
	printf("Me: %@, Server: %@",
		arptable[ARP_CLIENT].ipaddr.s_addr,
		arptable[ARP_SERVER].ipaddr.s_addr);
	if (BOOTP_DATA_ADDR->bootp_reply.bp_giaddr.s_addr)
		printf(", Relay: %@",
			BOOTP_DATA_ADDR->bootp_reply.bp_giaddr.s_addr);
	if (arptable[ARP_GATEWAY].ipaddr.s_addr)
		printf(", Gateway %@", arptable[ARP_GATEWAY].ipaddr.s_addr);
	putchar('\n');

#ifdef	MDEBUG
	printf("\n=>>"); getchar();
#endif

	/* Now use TFTP to load file */
#ifdef	DOWNLOAD_PROTO_NFS
	rpc_init();
#endif
#ifdef	DEFAULT_BOOTFILE
	kernel = KERNEL_BUF[0] != '\0' ? KERNEL_BUF : DEFAULT_BOOTFILE;
#else
	kernel = KERNEL_BUF;
#endif
	printf("Loading %@:%s ", arptable[ARP_SERVER].ipaddr, kernel);
	loadkernel(kernel); /* We don't return except on error */
	printf("Unable to load file.\n");
	interruptible_sleep(2);		/* lay off the server for a while */
	longjmp(restart_etherboot, -1);
}

/**************************************************************************
DEFAULT_NETMASK - Return default netmask for IP address
**************************************************************************/
static inline unsigned long default_netmask(void)
{
	int net = ntohl(arptable[ARP_CLIENT].ipaddr.s_addr) >> 24;
	if (net <= 127)
		return(htonl(0xff000000));
	else if (net < 192)
		return(htonl(0xffff0000));
	else
		return(htonl(0xffffff00));
}

/**************************************************************************
IP_TRANSMIT - Send an IP datagram
**************************************************************************/
static int await_arp(int ival, void *ptr,
	unsigned short ptype, struct iphdr *ip __unused, struct udphdr *udp __unused)
{
	struct	arprequest *arpreply;
	if (ptype != ARP)
		return 0;
	if (nic.packetlen < ETH_HLEN + sizeof(struct arprequest))
		return 0;
	arpreply = (struct arprequest *)&nic.packet[ETH_HLEN];

	if (arpreply->opcode != htons(ARP_REPLY)) 
		return 0;
	if (memcmp(arpreply->sipaddr, ptr, sizeof(in_addr)) != 0)
		return 0;
	memcpy(arptable[ival].node, arpreply->shwaddr, ETH_ALEN);
	return 1;
}

int ip_transmit(int len, const void *buf)
{
	unsigned long destip;
	struct iphdr *ip;
	struct arprequest arpreq;
	int arpentry, i;
	int retry;

	ip = (struct iphdr *)buf;
	destip = ip->dest.s_addr;
	if (destip == IP_BROADCAST) {
		eth_transmit(broadcast, IP, len, buf);
#ifdef MULTICAST_LEVEL1 
	} else if ((destip & htonl(MULTICAST_MASK)) == htonl(MULTICAST_NETWORK)) {
		unsigned char multicast[6];
		unsigned long hdestip;
		hdestip = ntohl(destip);
		multicast[0] = 0x01;
		multicast[1] = 0x00;
		multicast[2] = 0x5e;
		multicast[3] = (hdestip >> 16) & 0x7;
		multicast[4] = (hdestip >> 8) & 0xff;
		multicast[5] = hdestip & 0xff;
		eth_transmit(multicast, IP, len, buf);
#endif
	} else {
		if (((destip & netmask) !=
			(arptable[ARP_CLIENT].ipaddr.s_addr & netmask)) &&
			arptable[ARP_GATEWAY].ipaddr.s_addr)
				destip = arptable[ARP_GATEWAY].ipaddr.s_addr;
		for(arpentry = 0; arpentry<MAX_ARP; arpentry++)
			if (arptable[arpentry].ipaddr.s_addr == destip) break;
		if (arpentry == MAX_ARP) {
			printf("%@ is not in my arp table!\n", destip);
			return(0);
		}
		for (i = 0; i < ETH_ALEN; i++)
			if (arptable[arpentry].node[i])
				break;
		if (i == ETH_ALEN) {	/* Need to do arp request */
			arpreq.hwtype = htons(1);
			arpreq.protocol = htons(IP);
			arpreq.hwlen = ETH_ALEN;
			arpreq.protolen = 4;
			arpreq.opcode = htons(ARP_REQUEST);
			memcpy(arpreq.shwaddr, arptable[ARP_CLIENT].node, ETH_ALEN);
			memcpy(arpreq.sipaddr, &arptable[ARP_CLIENT].ipaddr, sizeof(in_addr));
			memset(arpreq.thwaddr, 0, ETH_ALEN);
			memcpy(arpreq.tipaddr, &destip, sizeof(in_addr));
			for (retry = 1; retry <= MAX_ARP_RETRIES; retry++) {
				long timeout;
				eth_transmit(broadcast, ARP, sizeof(arpreq),
					&arpreq);
				timeout = rfc2131_sleep_interval(TIMEOUT, retry);
				if (await_reply(await_arp, arpentry,
					arpreq.tipaddr, timeout)) goto xmit;
			}
			return(0);
		}
xmit:
		eth_transmit(arptable[arpentry].node, IP, len, buf);
	}
	return 1;
}

void build_ip_hdr(unsigned long destip, int ttl, int protocol, 
	int len, const void *buf)
{
	struct iphdr *ip;
	ip = (struct iphdr *)buf;
	ip->verhdrlen = 0x45;
	ip->service = 0;
	ip->len = htons(len);
	ip->ident = 0;
	ip->frags = 0;
	ip->ttl = ttl;
	ip->protocol = protocol;
	ip->chksum = 0;
	ip->src.s_addr = arptable[ARP_CLIENT].ipaddr.s_addr;
	ip->dest.s_addr = destip;
	ip->chksum = ipchksum((unsigned short *)buf, sizeof(struct iphdr));
}

void build_udp_hdr(unsigned long destip, 
	unsigned int srcsock, unsigned int destsock, int ttl,
	int len, const void *buf)
{
	struct iphdr *ip;
	struct udphdr *udp;
	ip = (struct iphdr *)buf;
	build_ip_hdr(destip, ttl, IP_UDP, len, buf);
	udp = (struct udphdr *)((char *)buf + sizeof(struct iphdr));
	udp->src = htons(srcsock);
	udp->dest = htons(destsock);
	udp->len = htons(len - sizeof(struct iphdr));
	udp->chksum = 0;
	if ((udp->chksum = htons(udpchksum(ip))) == 0)
		udp->chksum = 0xffff;
}


/**************************************************************************
UDP_TRANSMIT - Send an UDP datagram
**************************************************************************/
int udp_transmit(unsigned long destip, unsigned int srcsock,
	unsigned int destsock, int len, const void *buf)
{
	build_udp_hdr(destip, srcsock, destsock, 60, len, buf);
	return ip_transmit(len, buf);
}

/**************************************************************************
QDRAIN - clear the nic's receive queue
**************************************************************************/
static int await_qdrain(int ival __unused, void *ptr __unused,
	unsigned short ptype __unused, 
	struct iphdr *ip __unused, struct udphdr *udp __unused)
{
	return 0;
}

void rx_qdrain(void)
{
	/* Clear out the Rx queue first.  It contains nothing of interest,
	 * except possibly ARP requests from the DHCP/TFTP server.  We use
	 * polling throughout Etherboot, so some time may have passed since we
	 * last polled the receive queue, which may now be filled with
	 * broadcast packets.  This will cause the reply to the packets we are
	 * about to send to be lost immediately.  Not very clever.  */
	await_reply(await_qdrain, 0, NULL, 0);
}

#ifdef	DOWNLOAD_PROTO_TFTP
/**************************************************************************
TFTP - Download extended BOOTP data, or kernel image
**************************************************************************/
static int await_tftp(int ival, void *ptr __unused,
	unsigned short ptype __unused, struct iphdr *ip, struct udphdr *udp)
{
	if (!udp) {
		return 0;
	}
	if (arptable[ARP_CLIENT].ipaddr.s_addr != ip->dest.s_addr)
		return 0;
	if (ntohs(udp->dest) != ival)
		return 0;
	return 1;
}

int tftp(const char *name, int (*fnc)(unsigned char *, unsigned int, unsigned int, int))
{
	int             retry = 0;
	static unsigned short iport = 2000;
	unsigned short  oport;
	unsigned short  len, block = 0, prevblock = 0;
	int		bcounter = 0;
	struct tftp_t  *tr;
	struct tftpreq_t tp;
	int		rc;
	int		packetsize = TFTP_DEFAULTSIZE_PACKET;

	rx_qdrain();

	tp.opcode = htons(TFTP_RRQ);
	/* Warning: the following assumes the layout of bootp_t.
	   But that's fixed by the IP, UDP and BOOTP specs. */
	len = sizeof(tp.ip) + sizeof(tp.udp) + sizeof(tp.opcode) +
		sprintf((char *)tp.u.rrq, "%s%coctet%cblksize%c%d",
		name, 0, 0, 0, TFTP_MAX_PACKET) + 1;
	if (!udp_transmit(arptable[ARP_SERVER].ipaddr.s_addr, ++iport,
		TFTP_PORT, len, &tp))
		return (0);
	for (;;)
	{
		long timeout;
#ifdef	CONGESTED
		timeout = rfc2131_sleep_interval(block?TFTP_REXMT: TIMEOUT, retry);
#else
		timeout = rfc2131_sleep_interval(TIMEOUT, retry);
#endif
		if (!await_reply(await_tftp, iport, NULL, timeout))
		{
			if (!block && retry++ < MAX_TFTP_RETRIES)
			{	/* maybe initial request was lost */
				if (!udp_transmit(arptable[ARP_SERVER].ipaddr.s_addr,
					++iport, TFTP_PORT, len, &tp))
					return (0);
				continue;
			}
#ifdef	CONGESTED
			if (block && ((retry += TFTP_REXMT) < TFTP_TIMEOUT))
			{	/* we resend our last ack */
#ifdef	MDEBUG
				printf("<REXMT>\n");
#endif
				udp_transmit(arptable[ARP_SERVER].ipaddr.s_addr,
					iport, oport,
					TFTP_MIN_PACKET, &tp);
				continue;
			}
#endif
			break;	/* timeout */
		}
		tr = (struct tftp_t *)&nic.packet[ETH_HLEN];
		if (tr->opcode == ntohs(TFTP_ERROR))
		{
			printf("TFTP error %d (%s)\n",
			       ntohs(tr->u.err.errcode),
			       tr->u.err.errmsg);
			break;
		}

		if (tr->opcode == ntohs(TFTP_OACK)) {
			const char *p = tr->u.oack.data, *e;

			if (prevblock)		/* shouldn't happen */
				continue;	/* ignore it */
			len = ntohs(tr->udp.len) - sizeof(struct udphdr) - 2;
			if (len > TFTP_MAX_PACKET)
				goto noak;
			e = p + len;
			while (*p != '\0' && p < e) {
				if (!strcasecmp("blksize", p)) {
					p += 8;
					if ((packetsize = strtoul(p, &p, 10)) <
					    TFTP_DEFAULTSIZE_PACKET)
						goto noak;
					while (p < e && *p) p++;
					if (p < e)
						p++;
				}
				else {
				noak:
					tp.opcode = htons(TFTP_ERROR);
					tp.u.err.errcode = 8;
/*
 *	Warning: the following assumes the layout of bootp_t.
 *	But that's fixed by the IP, UDP and BOOTP specs.
 */
					len = sizeof(tp.ip) + sizeof(tp.udp) + sizeof(tp.opcode) + sizeof(tp.u.err.errcode) +
/*
 *	Normally bad form to omit the format string, but in this case
 *	the string we are copying from is fixed. sprintf is just being
 *	used as a strcpy and strlen.
 */
						sprintf((char *)tp.u.err.errmsg,
						"RFC1782 error") + 1;
					udp_transmit(arptable[ARP_SERVER].ipaddr.s_addr,
						     iport, ntohs(tr->udp.src),
						     len, &tp);
					return (0);
				}
			}
			if (p > e)
				goto noak;
			block = tp.u.ack.block = 0; /* this ensures, that */
						/* the packet does not get */
						/* processed as data! */
		}
		else if (tr->opcode == htons(TFTP_DATA)) {
			len = ntohs(tr->udp.len) - sizeof(struct udphdr) - 4;
			if (len > packetsize)	/* shouldn't happen */
				continue;	/* ignore it */
			block = ntohs(tp.u.ack.block = tr->u.data.block); }
		else /* neither TFTP_OACK nor TFTP_DATA */
			break;

		if ((block || bcounter) && (block != (unsigned short)(prevblock+1))) {
			/* Block order should be continuous */
			tp.u.ack.block = htons(block = prevblock);
		}
		tp.opcode = htons(TFTP_ACK);
		oport = ntohs(tr->udp.src);
		udp_transmit(arptable[ARP_SERVER].ipaddr.s_addr, iport,
			oport, TFTP_MIN_PACKET, &tp);	/* ack */
		if ((unsigned short)(block-prevblock) != 1) {
			/* Retransmission or OACK, don't process via callback
			 * and don't change the value of prevblock.  */
			continue;
		}
		prevblock = block;
		retry = 0;	/* It's the right place to zero the timer? */
		if ((rc = fnc(tr->u.data.download,
			      ++bcounter, len, len < packetsize)) <= 0)
			return(rc);
		if (len < packetsize)		/* End of data */
			return (1);
	}
	return (0);
}
#endif	/* DOWNLOAD_PROTO_TFTP */

#ifdef	RARP_NOT_BOOTP
/**************************************************************************
RARP - Get my IP address and load information
**************************************************************************/
static int await_rarp(int ival, void *ptr,
	unsigned short ptype, struct iphdr *ip, struct udphdr *udp)
{
	struct arprequest *arpreply;
	if (ptype != RARP)
		return;
	if (nic.packetlen < ETH_HLEN + sizeof(struct arprequest))
		return 0;
	arpreply = (struct arprequest *)&nic.pakcet[ETH_HLEN];
	if (arpreply->opcode != htons(RARP_REPLY))
		return 0;
	if (memcmp(arpreply->thwaddr, 
	if ((arpreply->opcode == htons(RARP_REPLY)) &&
		(memcmp(arpreply->thwaddr, ptr, ETH_ALEN) == 0)) {
		memcpy(arptable[ARP_SERVER].node, arpreply->shwaddr, ETH_ALEN);
		memcpy(&arptable[ARP_SERVER].ipaddr, arpreply->sipaddr, sizeof(in_addr));
		memcpy(&arptable[ARP_CLIENT].ipaddr, arpreply->tipaddr, sizeof(in_addr));
		return 1;
	}
	}
	return 0;
}

static int rarp(void)
{
	int retry;

	/* arp and rarp requests share the same packet structure. */
	struct arprequest rarpreq;

	memset(&rarpreq, 0, sizeof(rarpreq));

	rarpreq.hwtype = htons(1);
	rarpreq.protocol = htons(IP);
	rarpreq.hwlen = ETH_ALEN;
	rarpreq.protolen = 4;
	rarpreq.opcode = htons(RARP_REQUEST);
	memcpy(&rarpreq.shwaddr, arptable[ARP_CLIENT].node, ETH_ALEN);
	/* sipaddr is already zeroed out */
	memcpy(&rarpreq.thwaddr, arptable[ARP_CLIENT].node, ETH_ALEN);
	/* tipaddr is already zeroed out */

	for (retry = 0; retry < MAX_ARP_RETRIES; ++retry) {
		long timeout;
		eth_transmit(broadcast, RARP, sizeof(rarpreq), &rarpreq);

		timeout = rfc2131_sleep_interval(TIMEOUT, retry);
		if (await_reply(await_rarp, 0, rarpreq.shwaddr, timeout))
			break;
	}

	if (retry < MAX_ARP_RETRIES) {
		(void)sprintf(KERNEL_BUF, DEFAULT_KERNELPATH, arptable[ARP_CLIENT].ipaddr);

		return (1);
	}
	return (0);
}

#else

/**************************************************************************
BOOTP - Get my IP address and load information
**************************************************************************/
static int await_bootp(int ival __unused, void *ptr __unused,
	unsigned short ptype __unused, struct iphdr *ip __unused, 
	struct udphdr *udp)
{
	struct	bootp_t *bootpreply;
	if (!udp) {
		return 0;
	}
	bootpreply = (struct bootp_t *)&nic.packet[ETH_HLEN + 
		sizeof(struct iphdr) + sizeof(struct udphdr)];
	if (nic.packetlen < ETH_HLEN + sizeof(struct iphdr) + 
		sizeof(struct udphdr) + 
#ifdef NO_DHCP_SUPPORT
		sizeof(struct bootp_t)
#else
		sizeof(struct bootp_t) - DHCP_OPT_LEN
#endif	/* NO_DHCP_SUPPORT */
		) {
		return 0;
	}
	if (udp->dest != htons(BOOTP_CLIENT))
		return 0;
	if (bootpreply->bp_op != BOOTP_REPLY)
		return 0;
	if (bootpreply->bp_xid != xid)
		return 0;
	if (memcmp(&bootpreply->bp_siaddr, &zeroIP, sizeof(in_addr)) == 0)
		return 0;
#ifndef	DEFAULT_BOOTFILE
	if (bootpreply->bp_file[0] == '\0')
		return 0;
#endif
	if ((memcmp(broadcast, bootpreply->bp_hwaddr, ETH_ALEN) != 0) &&
		(memcmp(arptable[ARP_CLIENT].node, bootpreply->bp_hwaddr, ETH_ALEN) != 0)) {
		return 0;
	}
	arptable[ARP_CLIENT].ipaddr.s_addr = bootpreply->bp_yiaddr.s_addr;
#ifndef	NO_DHCP_SUPPORT
	dhcp_addr.s_addr = bootpreply->bp_yiaddr.s_addr;
#endif	/* NO_DHCP_SUPPORT */
	netmask = default_netmask();
	arptable[ARP_SERVER].ipaddr.s_addr = bootpreply->bp_siaddr.s_addr;
	memset(arptable[ARP_SERVER].node, 0, ETH_ALEN);  /* Kill arp */
	arptable[ARP_GATEWAY].ipaddr.s_addr = bootpreply->bp_giaddr.s_addr;
	memset(arptable[ARP_GATEWAY].node, 0, ETH_ALEN);  /* Kill arp */
	/* bootpreply->bp_file will be copied to KERNEL_BUF in the memcpy */
	memcpy((char *)BOOTP_DATA_ADDR, (char *)bootpreply, sizeof(struct bootpd_t));
	decode_rfc1533(BOOTP_DATA_ADDR->bootp_reply.bp_vend, 0,
#ifdef	NO_DHCP_SUPPORT
		BOOTP_VENDOR_LEN + MAX_BOOTP_EXTLEN, 
#else
		DHCP_OPT_LEN + MAX_BOOTP_EXTLEN, 
#endif	/* NO_DHCP_SUPPORT */
		1);
#ifdef	REQUIRE_VCI_ETHERBOOT
	if (!vci_etherboot)
		return (0);
#endif
	return(1);
}

static int bootp(void)
{
	int retry;
#ifndef	NO_DHCP_SUPPORT
	int reqretry;
#endif	/* NO_DHCP_SUPPORT */
	struct bootpip_t ip;
	unsigned long  starttime;

	memset(&ip, 0, sizeof(struct bootpip_t));
	ip.bp.bp_op = BOOTP_REQUEST;
	ip.bp.bp_htype = 1;
	ip.bp.bp_hlen = ETH_ALEN;
	starttime = currticks();
	/* Use lower 32 bits of node address, more likely to be
	   distinct than the time since booting */
	memcpy(&xid, &arptable[ARP_CLIENT].node[2], sizeof(xid));
	ip.bp.bp_xid = xid += htonl(starttime);
	memcpy(ip.bp.bp_hwaddr, arptable[ARP_CLIENT].node, ETH_ALEN);
#ifdef	NO_DHCP_SUPPORT
	memcpy(ip.bp.bp_vend, rfc1533_cookie, 5); /* request RFC-style options */
#else
	memcpy(ip.bp.bp_vend, rfc1533_cookie, sizeof rfc1533_cookie); /* request RFC-style options */
	memcpy(ip.bp.bp_vend + sizeof rfc1533_cookie, dhcpdiscover, sizeof dhcpdiscover);
	ip.bp.bp_vend[sizeof rfc1533_cookie + sizeof dhcpdiscover] = RFC1533_END;
#endif	/* NO_DHCP_SUPPORT */

	for (retry = 0; retry < MAX_BOOTP_RETRIES; ) {
		long timeout;

		rx_qdrain();

		udp_transmit(IP_BROADCAST, BOOTP_CLIENT, BOOTP_SERVER,
			sizeof(struct bootpip_t), &ip);
		timeout = rfc2131_sleep_interval(TIMEOUT, retry++);
#ifdef	NO_DHCP_SUPPORT
		if (await_reply(await_bootp, 0, NULL, timeout))
			return(1);
#else
		if (await_reply(await_bootp, 0, NULL, timeout)) {
			/* If not a DHCPOFFER then must be just a BOOTP reply,
			   be backward compatible with BOOTP then */
			if (dhcp_reply != DHCPOFFER)
				return(1);
			dhcp_reply = 0;
			memcpy(ip.bp.bp_vend, rfc1533_cookie, sizeof rfc1533_cookie);
			memcpy(ip.bp.bp_vend + sizeof rfc1533_cookie, dhcprequest, sizeof dhcprequest);
			/* Beware: the magic numbers 9 and 15 depend on
			   the layout of dhcprequest */
			memcpy(&ip.bp.bp_vend[9], &dhcp_server, sizeof(in_addr));
			memcpy(&ip.bp.bp_vend[15], &dhcp_addr, sizeof(in_addr));
			/* Append NIC IDs to end, in encapsulated option */
			ip.bp.bp_vend[sizeof rfc1533_cookie + sizeof dhcprequest] = RFC1533_VENDOR_ETHERBOOT_ENCAP;
			memcpy(&ip.bp.bp_vend[sizeof rfc1533_cookie + sizeof dhcprequest + 1], &nic.devid, NIC_ID_SIZE);
			ip.bp.bp_vend[sizeof rfc1533_cookie + sizeof dhcprequest + 1 + NIC_ID_SIZE] = RFC1533_END;
			for (reqretry = 0; reqretry < MAX_BOOTP_RETRIES; ) {
				udp_transmit(IP_BROADCAST, BOOTP_CLIENT, BOOTP_SERVER,
					sizeof(struct bootpip_t), &ip);
				dhcp_reply=0;
				timeout = rfc2131_sleep_interval(TIMEOUT, reqretry++);
				if (await_reply(await_bootp, 0, NULL, timeout))
					if (dhcp_reply == DHCPACK)
						return(1);
			}
		}
#endif	/* NO_DHCP_SUPPORT */
		ip.bp.bp_secs = htons((currticks()-starttime)/TICKS_PER_SEC);
	}
	return(0);
}
#endif	/* RARP_NOT_BOOTP */

/**************************************************************************
UDPCHKSUM - Checksum UDP Packet (one of the rare cases when assembly is
            actually simpler...)
 RETURNS: checksum, 0 on checksum error. This
          allows for using the same routine for RX and TX summing:
          RX  if (packet->udp.chksum && udpchksum(packet))
                  error("checksum error");
          TX  packet->udp.chksum=0;
              if (0==(packet->udp.chksum=udpchksum(packet)))
                  packet->udp.chksum=0xffff;
**************************************************************************/
static inline void dosum(unsigned short *start, unsigned int len, unsigned short *sum)
{
	__asm__ __volatile__(
	"clc\n"
	"1:\tlodsw\n\t"
	"xchg %%al,%%ah\n\t"	/* convert to host byte order */
	"adcw %%ax,%0\n\t"	/* add carry of previous iteration */
	"loop 1b\n\t"
	"adcw $0,%0"		/* add carry of last iteration */
	: "=b" (*sum), "=S"(start), "=c"(len)
	: "0"(*sum), "1"(start), "2"(len)
	: "ax", "cc"
	);
}

/* UDP sum:
 * proto, src_ip, dst_ip, udp_dport, udp_sport, 2*udp_len, payload
 */
static unsigned short udpchksum(struct iphdr *packet)
{
	char *ptr = (char *) packet;
	int len = ntohs(packet->len);
	unsigned short rval;

	/* add udplength + protocol number */
	rval = (len - sizeof(struct iphdr)) + IP_UDP;

	/* pad to an even number of bytes */
	if (len % 2) {
		((char *) packet)[len++] = 0;
	}

	/* sum over src/dst ipaddr + udp packet */
	len -= (char *) &packet->src - (char *) packet;
	dosum((unsigned short *) &packet->src, len >> 1, &rval);

	/* take one's complement */
	return (~rval);
}

/**************************************************************************
AWAIT_REPLY - Wait until we get a response for our request
************f**************************************************************/
int await_reply(int (*reply)(int ival, void *ptr, 
		unsigned short ptype, struct iphdr *ip, struct udphdr *udp), 
	int ival, void *ptr, int timeout)
{
	unsigned long time, now;
	struct	iphdr *ip;
	struct	udphdr *udp;
	unsigned short ptype;
	int i;
	int result;

	time = timeout + currticks();
	/* The timeout check is done below.  The timeout is only checked if
	 * there is no packet in the Rx queue.  This assumes that eth_poll()
	 * needs a negligible amount of time.  */
	for (;;) {
#ifdef MULTICAST_LEVEL2
		now = currticks();
		for(i = 0; i < MAX_IGMP; i++) {
			if (igmptable[i].time && (now >= igmptable[i].time)) {
				struct igmp igmp;
				build_ip_hdr(igmptable[i].group.s_addr, 
					1, IP_IGMP, sizeof(igmp), &igmp);
				igmp.type_ver = IGMP_REPORT;
				igmp.dummy = 0;
				igmp.chksum = 0;
				igmp.group.s_addr = igmptable[i].group.s_addr;
				igmp.chksum = ipchksum((unsigned short *)&igmp.type_ver, 8);
				ip_transmit(sizeof(igmp), &igmp);
#ifdef	MDEBUG
				printf("Sent IGMP report to: %@\n", igmp.group.s_addr);
#endif			       
				/* Don't send another igmp report until asked */
				igmptable[i].time = 0;
			}
		}
#endif
		result = eth_poll();
		if (result == 0) {
			/* We don't have anything */
		
			/* Check for abort key only if the Rx queue is empty -
			 * as long as we have something to process, don't
			 * assume that something failed.  It is unlikely that
			 * we have no processing time left between packets.  */
			if (
#ifdef FREEBSD_PXEEMU
			    pxeemu_nbp_active == 0 &&
#endif
			    iskey() && (getchar() == ESC))
				longjmp(restart_etherboot, -1);
			/* Do the timeout after at least a full queue walk.  */
			if ((timeout == 0) || (currticks() > time)) {
				break;
			}
			continue;
		}
		
		/* We have something! */

		/* Find the Ethernet packet type */
		if (nic.packetlen >= ETH_HLEN) {
			ptype = ((unsigned short) nic.packet[12]) << 8
				| ((unsigned short) nic.packet[13]);
		} else continue; /* what else could we do with it? */
		/* Verify an IP header */
		ip = 0;
		if ((ptype == IP) && (nic.packetlen >= ETH_HLEN + sizeof(struct iphdr))) {
			ip = (struct iphdr *)&nic.packet[ETH_HLEN];
			if (ip->verhdrlen != 0x45)
				continue;
			if (ipchksum((unsigned short *)ip, sizeof(struct iphdr)) != 0)
				continue;
			if (ip->frags & htons(0x3FFF)) {
				static int warned_fragmentation = 0;
				if (!warned_fragmentation) {
					printf("ALERT: got a fragmented packet - reconfigure your server\n");
					warned_fragmentation = 1;
				}
				continue;
			}
		}
		udp = 0;
		if (ip && (ip->protocol == IP_UDP) && 
			(nic.packetlen >= 
				ETH_HLEN + sizeof(struct iphdr) + sizeof(struct udphdr))) {
			udp = (struct udphdr *)&nic.packet[ETH_HLEN + sizeof(struct iphdr)];
			if (udp->chksum && udpchksum(ip)) {
				printf("UDP checksum error\n");
				continue;
			}
		}
		result = reply(ival, ptr, ptype, ip, udp);
		if (result > 0) {
			return result;
		}
		
		/* If it isn't a packet the upper layer wants see if there is a default
		 * action.  This allows us reply to arp and igmp queryies.
		 */
		if ((ptype == ARP) &&
			(nic.packetlen >= ETH_HLEN + sizeof(struct arprequest))) {
			struct	arprequest *arpreply;
			unsigned long tmp;
		
			arpreply = (struct arprequest *)&nic.packet[ETH_HLEN];
			memcpy(&tmp, arpreply->tipaddr, sizeof(in_addr));
			if ((arpreply->opcode == htons(ARP_REQUEST)) &&
				(tmp == arptable[ARP_CLIENT].ipaddr.s_addr)) {
				arpreply->opcode = htons(ARP_REPLY);
				memcpy(arpreply->tipaddr, arpreply->sipaddr, sizeof(in_addr));
				memcpy(arpreply->thwaddr, arpreply->shwaddr, ETH_ALEN);
				memcpy(arpreply->sipaddr, &arptable[ARP_CLIENT].ipaddr, sizeof(in_addr));
				memcpy(arpreply->shwaddr, arptable[ARP_CLIENT].node, ETH_ALEN);
				eth_transmit(arpreply->thwaddr, ARP,
					sizeof(struct  arprequest),
					arpreply);
#ifdef	MDEBUG
				memcpy(&tmp, arpreply->tipaddr, sizeof(in_addr));
				printf("Sent ARP reply to: %@\n",tmp);
#endif	/* MDEBUG */
			}
		}
#ifdef MULTICAST_LEVEL2
		if (ip && (ip->protocol == IP_IGMP) && 
			nic.packetlen >= ETH_HLEN + sizeof(struct igmp)) {
			struct igmp * igmp;
			igmp = (struct igmp *)&nic.packet[ETH_HLEN];
			if (ipchksum((unsigned short *)&igmp->type_ver, 8) != 0)
				continue;
			if ((igmp->type_ver == IGMP_QUERY) && 
				(ip->dest.s_addr == htonl(GROUP_ALL_HOSTS))) {
#ifdef	MDEBUG
				printf("Received IGMP query for: %@\n", igmp->group.s_addr);
#endif			       
				for(i = 0; i < MAX_IGMP; i++) {
					if ((igmptable[i].group.s_addr == igmp->group.s_addr) &&
						igmptable[i].time == 0) {
						igmptable[i].time = currticks() + 
							rfc1112_sleep_interval(IGMP_INTERVAL,0);
						break;
					}
				}
			}
			if ((igmp->type_ver == IGMP_REPORT) && 
				(ip->dest.s_addr == igmp->group.s_addr)) {
#ifdef	MDEBUG
				printf("Received IGMP report for: %@\n", igmp->group.s_addr);
#endif			       
				for(i = 0; i < MAX_IGMP; i++) {
					if ((igmptable[i].group.s_addr == igmp->group.s_addr) &&
						igmptable[i].time != 0) {
						igmptable[i].time = 0;
					}
				}
			}
		}
			
#endif /* MULTICAST_LEVEL2 */
	}
	return(0);
}

#ifdef	REQUIRE_VCI_ETHERBOOT
/**************************************************************************
FIND_VCI_ETHERBOOT - Looks for "Etherboot" in Vendor Encapsulated Identifiers
On entry p points to byte count of VCI options
**************************************************************************/
static int find_vci_etherboot(unsigned char *p)
{
	unsigned char	*end = p + 1 + *p;

	for (p++; p < end; ) {
		if (*p == RFC2132_VENDOR_CLASS_ID) {
			if (strncmp("Etherboot", p + 2, sizeof("Etherboot") - 1) == 0)
				return (1);
		} else if (*p == RFC1533_END)
			return (0);
		p += TAG_LEN(p) + 2;
	}
	return (0);
}
#endif	/* REQUIRE_VCI_ETHERBOOT */

/**************************************************************************
DECODE_RFC1533 - Decodes RFC1533 header
**************************************************************************/
int decode_rfc1533(unsigned char *p, unsigned int block, unsigned int len, int eof)
{
	static unsigned char *extdata = NULL, *extend = NULL;
	unsigned char        *extpath = NULL;
	unsigned char        *endp;
	static unsigned char in_encapsulated_options = 0;

#ifdef	REQUIRE_VCI_ETHERBOOT
	vci_etherboot = 0;
#endif
	if (eof == -1) {
		/* Encapsulated option block */
		endp = p + len;
	}
	else if (block == 0) {
		end_of_rfc1533 = NULL;
#ifdef	IMAGE_FREEBSD
		/* yes this is a pain FreeBSD uses this for swap, however,
		   there are cases when you don't want swap and then
		   you want this set to get the extra features so lets
		   just set if dealing with FreeBSD.  I haven't run into
		   any troubles with this but I have without it
		*/
		vendorext_isvalid = 1;
#ifdef FREEBSD_KERNEL_ENV
		memcpy(freebsd_kernel_env, FREEBSD_KERNEL_ENV,
		       sizeof(FREEBSD_KERNEL_ENV));
		/* FREEBSD_KERNEL_ENV had better be a string constant */
#else
		freebsd_kernel_env[0]='\0';
#endif
#else
		vendorext_isvalid = 0;
#endif
		if (memcmp(p, rfc1533_cookie, 4))
			return(0); /* no RFC 1533 header found */
		p += 4;
		endp = p + len;
	} else {
		if (block == 1) {
			if (memcmp(p, rfc1533_cookie, 4))
				return(0); /* no RFC 1533 header found */
			p += 4;
			len -= 4; }
		if (extend + len <= (unsigned char *)&(BOOTP_DATA_ADDR->bootp_extension[MAX_BOOTP_EXTLEN])) {
			memcpy(extend, p, len);
			extend += len;
		} else {
			printf("Overflow in vendor data buffer! Aborting...\n");
			*extdata = RFC1533_END;
			return(0);
		}
		p = extdata; endp = extend;
	}
	if (!eof)
		return 1;
	while (p < endp) {
		unsigned char c = *p;
		if (c == RFC1533_PAD) {
			p++;
			continue;
		}
		else if (c == RFC1533_END) {
			end_of_rfc1533 = endp = p;
			continue;
		}
		else if (NON_ENCAP_OPT c == RFC1533_NETMASK)
			memcpy(&netmask, p+2, sizeof(in_addr));
		else if (NON_ENCAP_OPT c == RFC1533_GATEWAY) {
			/* This is a little simplistic, but it will
			   usually be sufficient.
			   Take only the first entry */
			if (TAG_LEN(p) >= sizeof(in_addr))
				memcpy(&arptable[ARP_GATEWAY].ipaddr, p+2, sizeof(in_addr));
		}
		else if (c == RFC1533_EXTENSIONPATH)
			extpath = p;
#ifndef	NO_DHCP_SUPPORT
#ifdef	REQUIRE_VCI_ETHERBOOT
		else if (NON_ENCAP_OPT c == RFC1533_VENDOR) {
			vci_etherboot = find_vci_etherboot(p+1);
#ifdef	MDEBUG
			printf("vci_etherboot %d\n", vci_etherboot);
#endif
		}
#endif	/* REQUIRE_VCI_ETHERBOOT */
		else if (NON_ENCAP_OPT c == RFC2132_MSG_TYPE)
			dhcp_reply=*(p+2);
		else if (NON_ENCAP_OPT c == RFC2132_SRV_ID)
			memcpy(&dhcp_server, p+2, sizeof(in_addr));
#endif	/* NO_DHCP_SUPPORT */
		else if (NON_ENCAP_OPT c == RFC1533_HOSTNAME) {
			hostname = p + 2;
			hostnamelen = *(p + 1);
		}
		else if (ENCAP_OPT c == RFC1533_VENDOR_MAGIC
			 && TAG_LEN(p) >= 6 &&
			  !memcmp(p+2,vendorext_magic,4) &&
			  p[6] == RFC1533_VENDOR_MAJOR
			)
			vendorext_isvalid++;
		else if (NON_ENCAP_OPT c == RFC1533_VENDOR_ETHERBOOT_ENCAP) {
			in_encapsulated_options = 1;
			decode_rfc1533(p+2, 0, TAG_LEN(p), -1);
			in_encapsulated_options = 0;
		}
#ifdef	IMAGE_FREEBSD
		else if (ENCAP_OPT c == RFC1533_VENDOR_HOWTO)
			freebsd_howto = ((p[2]*256+p[3])*256+p[4])*256+p[5];
		else if (ENCAP_OPT c == RFC1533_VENDOR_KERNEL_ENV){
			if(*(p + 1) < sizeof(freebsd_kernel_env)){
				memcpy(freebsd_kernel_env,p+2,*(p+1));
			}else{
				printf("Only support %d bytes in Kernel Env %d\n",sizeof(freebsd_kernel_env));
			}
		}
#endif
		else {
#if	0
			unsigned char *q;
			printf("Unknown RFC1533-tag ");
			for(q=p;q<p+2+TAG_LEN(p);q++)
				printf("%hhX ",*q);
			putchar('\n');
#endif
		}
		p += TAG_LEN(p) + 2;
	}
	extdata = extend = endp;
	if (block <= 0 && extpath != NULL) {
		char fname[64];
		memcpy(fname, extpath+2, TAG_LEN(extpath));
		fname[(int)TAG_LEN(extpath)] = '\0';
		printf("Loading BOOTP-extension file: %s\n",fname);
		download(fname, decode_rfc1533);
	}
	return 1;	/* proceed with next block */
}

/**************************************************************************
IPCHKSUM - Checksum IP Header
**************************************************************************/
uint16_t ipchksum(void *p, int len)
{
	unsigned long sum = 0;
	uint16_t *ip = p;
	while (len > 1) {
		len -= 2;
		sum += *(ip++);
		if (sum > 0xFFFF)
			sum -= 0xFFFF;
	}
	if (len) {
		uint8_t *ptr = (void *)ip;
		sum += *ptr;
		if (sum > 0xFFFF)
			sum -= 0xFFFF;
	}
	return((~sum) & 0x0000FFFF);
}

/**************************************************************************
RANDOM - compute a random number between 0 and 2147483647L or 2147483562?
**************************************************************************/
long random(void)
{
	static long seed = 0;
	long q;
	if (!seed) /* Initialize linear congruential generator */
		seed = currticks() + *(long *)&arptable[ARP_CLIENT].node
		       + ((short *)arptable[ARP_CLIENT].node)[2];
	/* simplified version of the LCG given in Bruce Schneier's
	   "Applied Cryptography" */
	q = seed/53668;
	if ((seed = 40014*(seed-53668*q) - 12211*q) < 0) seed += 2147483563L;
	return seed;
}

/* FIXME double check TWO_SECOND_DIVISOR */
#define TWO_SECOND_DIVISOR (RAND_MAX/TICKS_PER_SEC)
/**************************************************************************
RFC2131_SLEEP_INTERVAL - sleep for expotentially longer times (base << exp) +- 1 sec)
**************************************************************************/
long rfc2131_sleep_interval(int base, int exp)
{
	unsigned long tmo;

#ifdef BACKOFF_LIMIT
	if (exp > BACKOFF_LIMIT)
		exp = BACKOFF_LIMIT;
#endif
	tmo = (base << exp) + (TICKS_PER_SEC - (random()/TWO_SECOND_DIVISOR));
	return tmo;
}

#ifdef MULTICAST_LEVEL2
/**************************************************************************
RFC1112_SLEEP_INTERVAL - sleep for expotentially longer times, up to (base << exp)
**************************************************************************/
long rfc1112_sleep_interval(int base, int exp)
{
	unsigned long divisor, tmo;
#ifdef BACKOFF_LIMIT
	if (exp > BACKOFF_LIMIT)
		exp = BACKOFF_LIMIT;
#endif
	divisor = RAND_MAX/(base << exp);
	tmo = random()/divisor;
	return tmo;
}
#endif /* MULTICAST_LEVEL_2 */

/**************************************************************************
CLEANUP - shut down networking and console so that the OS may be called 
**************************************************************************/
void cleanup(void)
{
#ifdef	DOWNLOAD_PROTO_NFS
	nfs_umountall(ARP_SERVER);
#endif
	/* Stop receiving packets */
	eth_reset();
	eth_disable();
#ifdef DOWNLOAD_PROTO_DISK
	disk_disable();
#endif
}

/*
 * Local variables:
 *  c-basic-offset: 8
 * End:
 */
