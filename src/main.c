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

**************************************************************************/

/* #define MDEBUG */

#include "etherboot.h"
#include "nic.h"

jmpbuf			restart_etherboot;
struct arptable_t	arptable[MAX_ARP];
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

#ifdef	IMAGE_MENU
static char *imagelist[RFC1533_VENDOR_NUMOFIMG];
static int useimagemenu;
int menutmo,menudefault;
unsigned char *defparams = NULL;
int defparams_max = 0;
#endif

#ifdef	MOTD
char *motd[RFC1533_VENDOR_NUMOFMOTD];
#endif

#ifdef	IMAGE_FREEBSD
int freebsd_howto = 0;
char freebsd_kernel_env[256];
#endif

#ifndef	BOOTP_DATA_AT_0x93C00
struct bootpd_t bootp_data;
#endif

#ifdef	NO_DHCP_SUPPORT
char    rfc1533_cookie[5] = { RFC1533_COOKIE, RFC1533_END };
#else	/* !NO_DHCP_SUPPORT */
static int dhcp_reply;
static in_addr dhcp_server = { 0L };
static in_addr dhcp_addr = { 0L };
unsigned char rfc1533_cookie[] = { RFC1533_COOKIE};
unsigned char rfc1533_end[] = {RFC1533_END };
static const unsigned char dhcpdiscover[] = {
	RFC2132_MSG_TYPE,1,DHCPDISCOVER,
	RFC2132_MAX_SIZE,2,	/* request as much as we can */
	sizeof(struct bootpd_t) / 256, sizeof(struct bootpd_t) % 256,
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
	sizeof(struct bootpd_t) / 256, sizeof(struct bootpd_t) % 256,
	RFC2132_VENDOR_CLASS_ID,13,'E','t','h','e','r','b','o','o','t',
	'-',VERSION_MAJOR+'0','.',VERSION_MINOR+'0',
	/* request parameters */
	RFC2132_PARAM_LIST,
#ifdef	IMAGE_FREEBSD
	/* 5 standard + 6 vendortags + 8 motd + 16 menu items */
	5 + 6 + 8 + 16,
#else
	/* 5 standard + 5 vendortags + 8 motd + 16 menu items */
	5 + 5 + 8 + 16,
#endif
	/* Standard parameters */
	RFC1533_NETMASK, RFC1533_GATEWAY,
	RFC1533_HOSTNAME,RFC1533_VENDOR,
	RFC1533_ROOTPATH,	/* only passed to the booted image */
	/* Etherboot vendortags */
	RFC1533_VENDOR_MAGIC,
	RFC1533_VENDOR_ADDPARM,
	RFC1533_VENDOR_ETHDEV,
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
static int downloadkernel(unsigned char *data, int block, int len, int eof);
static unsigned short ipchksum(unsigned short *ip, int len);
static unsigned short udpchksum(struct iphdr *packet);

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
	int i;

	for (p = edata; p < end; p++)
		*p = 0;	/* Zero BSS */

#ifdef	CONSOLE_SERIAL
	(void)serial_init();
#endif

#ifdef	DELIMITERLINES
	for (i=0; i<80; i++) putchar('=');
#endif

	rom = *(struct rom_info *)ROM_INFO_LOCATION;
	printf("ROM segment %#hx length %#hx reloc %#hx\n", rom.rom_segment,
		rom.rom_length << 1, ((unsigned long)_start) >> 4);

	gateA20_set();
	print_config();
	/* -1:	timeout or ESC
	   -2:	error return from loader
	   0:	retry booting bootp and tftp
	   1:   retry tftp with possibly modified bootp reply
	   2:   retry bootp and tftp
	   255: exit Etherboot
	*/
	while(1) {
		i = setjmp(restart_etherboot);
		if (i == 0) {
			/* We just called setjmp ... */
			ask_boot();
			try_floppy_first();
			if (!eth_probe()) {
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
			load();
		}
		else if (i == 2) {
			load_configuration();
			load();
		}
		else if (i == 255) {
			exit(0);
			longjmp(restart_etherboot, -1);
		}
		else {
#if	defined(ANSIESC) && defined(CONSOLE_CRT)
			ansi_reset();
#endif
			printf("<abort>\n");
#ifdef EMERGENCYDISKBOOT
			exit(0);
#endif
			/* Reset the adapter to it's default state */
			eth_reset();
			/* Fall through and restart asking for files */
		}
	}
}


/**************************************************************************
LOADKERNEL - Try to load kernel image
**************************************************************************/
#ifndef	CAN_BOOT_DISK
#define loadkernel(s) download((s), downloadkernel)
#else
static int loadkernel(const char *fname)
{
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
		return(bootdisk(dev,part));
	}
nodisk:
	return download(fname, downloadkernel);
}
#endif

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

#ifdef	MOTD
	if (vendorext_isvalid)
		show_motd();
#endif
	/* Now use TFTP to load file */
#ifdef	IMAGE_MENU
	if (vendorext_isvalid && useimagemenu) {
		selectImage(imagelist);
	}
#endif
#ifdef	DOWNLOAD_PROTO_NFS
	rpc_init();
#endif
	kernel = KERNEL_BUF[0] != '\0' ? KERNEL_BUF : DEFAULT_BOOTFILE;
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
UDP_TRANSMIT - Send a UDP datagram
**************************************************************************/
int udp_transmit(unsigned long destip, unsigned int srcsock,
	unsigned int destsock, int len, const void *buf)
{
	struct iphdr *ip;
	struct udphdr *udp;
	struct arprequest arpreq;
	int arpentry, i;
	int retry;

	ip = (struct iphdr *)buf;
	udp = (struct udphdr *)((char *)buf + sizeof(struct iphdr));
	ip->verhdrlen = 0x45;
	ip->service = 0;
	ip->len = htons(len);
	ip->ident = 0;
	ip->frags = 0;
	ip->ttl = 60;
	ip->protocol = IP_UDP;
	ip->chksum = 0;
	ip->src.s_addr = arptable[ARP_CLIENT].ipaddr.s_addr;
	ip->dest.s_addr = destip;
	ip->chksum = ipchksum((unsigned short *)buf, sizeof(struct iphdr));
	udp->src = htons(srcsock);
	udp->dest = htons(destsock);
	udp->len = htons(len - sizeof(struct iphdr));
	udp->chksum = 0;
	if ((udp->chksum = htons(udpchksum(ip))) == 0)
		udp->chksum = 0xffff;
	if (destip == IP_BROADCAST) {
		eth_transmit(broadcast, IP, len, buf);
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
				if (await_reply(AWAIT_ARP, arpentry,
					arpreq.tipaddr, timeout)) goto xmit;
			}
			return(0);
		}
xmit:
		eth_transmit(arptable[arpentry].node, IP, len, buf);
	}
	return(1);
}

/**************************************************************************
DOWNLOADKERNEL - Try to load file
**************************************************************************/
static int downloadkernel(unsigned char *data, int block, int len, int eof)
{
#ifdef	SIZEINDICATOR
	static int rlen = 0;

	if (!(block % 4) || eof) {
		int size;
		size = ((block-1) * rlen + len) / 1024;

		putchar('\b');
		putchar('\b');
		putchar('\b');
		putchar('\b');

		putchar('0' + (size/1000)%10);
		putchar('0' + (size/100)%10);
		putchar('0' + (size/10)%10);
		putchar('0' + (size/1)%10);
	}
#endif
	if (block == 1)
	{
#ifdef	SIZEINDICATOR
		rlen=len;
#endif
		if (!eof && (
#ifdef	TAGGED_IMAGE
		    *((unsigned long *)data) == 0x1B031336L ||
#endif
#ifdef	ELF_IMAGE
		    *((unsigned long *)data) == 0x464C457FL ||
#endif
#ifdef	AOUT_IMAGE
		    *((unsigned short *)data) == 0x010BL ||
#endif
		    0))
		{
			;
		}
#if	0	/* is this a vestige of non-tagged images? */
		else if (eof)
		{
			memcpy(config_buffer, data, len);
			config_buffer[len] = 0;
			return (1); /* done */
		}
#endif
		else
		{
			printf("error: not a valid image\n");
			return(0); /* error */
		}
	}
	if (len != 0) {
		if (!os_download(block, data, len))
			return(0); /* error */
	}
	if (eof) {
		os_download(block+1, data, 0); /* does not return */
		return(0); /* error */
	}
	return(-1); /* there is more data */
}

#ifdef	DOWNLOAD_PROTO_TFTP
/**************************************************************************
TFTP - Download extended BOOTP data, or kernel image
**************************************************************************/
int tftp(const char *name, int (*fnc)(unsigned char *, int, int, int))
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

	/* Clear out the Rx queue first.  It contains nothing of interest,
	 * except possibly ARP requests from the DHCP/TFTP server.  We use
	 * polling throughout Etherboot, so some time may have passed since we
	 * last polled the receive queue, which may now be filled with
	 * broadcast packets.  This will cause the reply to the packets we are
	 * about to send to be lost immediately.  Not very clever.  */
	await_reply(AWAIT_QDRAIN, 0, NULL, 0);

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
		if (!await_reply(AWAIT_TFTP, iport, NULL, timeout))
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
			char *p = tr->u.oack.data, *e;

			if (prevblock)		/* shouldn't happen */
				continue;	/* ignore it */
			len = ntohs(tr->udp.len) - sizeof(struct udphdr) - 2;
			if (len > TFTP_MAX_PACKET)
				goto noak;
			e = p + len;
			while (*p != '\0' && p < e) {
				if (!strcasecmp("blksize", p)) {
					p += 8;
					if ((packetsize = getdec(&p)) <
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

		if ((block || bcounter) && (block != prevblock+1)) {
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
			      ++bcounter, len, len < packetsize)) >= 0)
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
		if (await_reply(AWAIT_RARP, 0, rarpreq.shwaddr, timeout))
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
	memcpy(ip.bp.bp_vend + sizeof rfc1533_cookie + sizeof dhcpdiscover, rfc1533_end, sizeof rfc1533_end);
#endif	/* NO_DHCP_SUPPORT */

	for (retry = 0; retry < MAX_BOOTP_RETRIES; ) {
		long timeout;

		/* Clear out the Rx queue first.  It contains nothing of
		 * interest, except possibly ARP requests from the DHCP/TFTP
		 * server.  We use polling throughout Etherboot, so some time
		 * may have passed since we last polled the receive queue,
		 * which may now be filled with broadcast packets.  This will
		 * cause the reply to the packets we are about to send to be
		 * lost immediately.  Not very clever.  */
		await_reply(AWAIT_QDRAIN, 0, NULL, 0);

		udp_transmit(IP_BROADCAST, BOOTP_CLIENT, BOOTP_SERVER,
			sizeof(struct bootpip_t), &ip);
		timeout = rfc2131_sleep_interval(TIMEOUT, retry++);
#ifdef	NO_DHCP_SUPPORT
		if (await_reply(AWAIT_BOOTP, 0, NULL, timeout))
			return(1);
#else
		if (await_reply(AWAIT_BOOTP, 0, NULL, timeout)) {
			/* If not a DHCPOFFER then must be just a BOOTP reply,
			   be backward compatible with BOOTP then */
			if (dhcp_reply != DHCPOFFER)
				return(1);
			dhcp_reply = 0;
			memcpy(ip.bp.bp_vend, rfc1533_cookie, sizeof rfc1533_cookie);
			memcpy(ip.bp.bp_vend + sizeof rfc1533_cookie, dhcprequest, sizeof dhcprequest);
			memcpy(ip.bp.bp_vend + sizeof rfc1533_cookie + sizeof dhcprequest, rfc1533_end, sizeof rfc1533_end);
			/* Beware: the magic numbers 9 and 15 depend on
			   the layout of dhcprequest */
			memcpy(ip.bp.bp_vend + 9, &dhcp_server, sizeof(in_addr));
			memcpy(ip.bp.bp_vend + 15, &dhcp_addr, sizeof(in_addr));
			for (reqretry = 0; reqretry < MAX_BOOTP_RETRIES; ) {
				udp_transmit(IP_BROADCAST, BOOTP_CLIENT, BOOTP_SERVER,
					sizeof(struct bootpip_t), &ip);
				dhcp_reply=0;
				timeout = rfc2131_sleep_interval(TIMEOUT, reqretry++);
				if (await_reply(AWAIT_BOOTP, 0, NULL, timeout))
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
                  packet->upd.chksum=0xffff;
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
**************************************************************************/
int await_reply(int type, int ival, void *ptr, int timeout)
{
	unsigned long time;
	struct	iphdr *ip;
	struct	udphdr *udp;
	struct	arprequest *arpreply;
	struct	bootp_t *bootpreply;
	struct	rpc_t *rpc;
	unsigned short ptype;

	unsigned int protohdrlen = ETH_HLEN + sizeof(struct iphdr) +
				sizeof(struct udphdr);
	time = timeout + currticks();
	/* The timeout check is done below.  The timeout is only checked if
	 * there is no packet in the Rx queue.  This assumes that eth_poll()
	 * needs a negligible amount of time.  */
	for (;;) {
		if (eth_poll()) {	/* We have something! */
					/* Check for ARP - No IP hdr */
			if (nic.packetlen >= ETH_HLEN) {
				ptype = ((unsigned short) nic.packet[12]) << 8
					| ((unsigned short) nic.packet[13]);
			} else continue; /* what else could we do with it? */
			if ((nic.packetlen >= ETH_HLEN +
				sizeof(struct arprequest)) &&
			   (ptype == ARP) ) {
				unsigned long tmp;

				arpreply = (struct arprequest *)
					&nic.packet[ETH_HLEN];
				if ((arpreply->opcode == htons(ARP_REPLY)) &&
				   !memcmp(arpreply->sipaddr, ptr, sizeof(in_addr)) &&
				   (type == AWAIT_ARP)) {
					memcpy(arptable[ival].node, arpreply->shwaddr, ETH_ALEN);
					return(1);
				}
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
				continue;
			}

			if (type == AWAIT_QDRAIN) {
				continue;
			}

					/* Check for RARP - No IP hdr */
			if ((type == AWAIT_RARP) &&
			   (nic.packetlen >= ETH_HLEN +
				sizeof(struct arprequest)) &&
			   (ptype == RARP)) {
				arpreply = (struct arprequest *)
					&nic.packet[ETH_HLEN];
				if ((arpreply->opcode == htons(RARP_REPLY)) &&
				   !memcmp(arpreply->thwaddr, ptr, ETH_ALEN)) {
					memcpy(arptable[ARP_SERVER].node, arpreply->shwaddr, ETH_ALEN);
					memcpy(& arptable[ARP_SERVER].ipaddr, arpreply->sipaddr, sizeof(in_addr));
					memcpy(& arptable[ARP_CLIENT].ipaddr, arpreply->tipaddr, sizeof(in_addr));
					return(1);
				}
				continue;
			}

					/* Anything else has IP header */
			if ((nic.packetlen < protohdrlen) ||
			   (ptype != IP) ) continue;
			ip = (struct iphdr *)&nic.packet[ETH_HLEN];
			if ((ip->verhdrlen != 0x45) ||
				ipchksum((unsigned short *)ip, sizeof(struct iphdr)) ||
				(ip->protocol != IP_UDP)) continue;
/*
	- Till Straumann <Till.Straumann@TU-Berlin.de>
	  added udp checksum (safer on a wireless link)
	  added fragmentation check: I had a corrupted image
	  in memory due to fragmented TFTP packets - took me
	  3 days to find the cause for this :-(
*/
			/* If More Fragments bit and Fragment Offset field
			   are non-zero then packet is fragmented */
			if (ip->frags & htons(0x3FFF)) {
				printf("ALERT: got a fragmented packet - reconfigure your server\n");
				continue;
			}
			udp = (struct udphdr *)&nic.packet[ETH_HLEN +
				sizeof(struct iphdr)];
			if (udp->chksum && udpchksum(ip)) {
				printf("UDP checksum error\n");
				continue;
			}

					/* BOOTP ? */
			bootpreply = (struct bootp_t *)&nic.packet[ETH_HLEN + sizeof(struct iphdr) + sizeof(struct udphdr)];
			if ((type == AWAIT_BOOTP) &&
			   (nic.packetlen >= (ETH_HLEN + sizeof(struct iphdr) + sizeof(struct udphdr) +
#ifdef	NO_DHCP_SUPPORT
			     sizeof(struct bootp_t))) &&
#else
			     sizeof(struct bootp_t))-DHCP_OPT_LEN) &&
#endif	/* NO_DHCP_SUPPORT */
			   (udp->dest == htons(BOOTP_CLIENT)) &&
			   (bootpreply->bp_op == BOOTP_REPLY) &&
			   (bootpreply->bp_xid == xid) &&
			   (memcmp(broadcast, bootpreply->bp_hwaddr, ETH_ALEN) ==0 ||
			    memcmp(arptable[ARP_CLIENT].node, bootpreply->bp_hwaddr, ETH_ALEN) == 0)) {
				arptable[ARP_CLIENT].ipaddr.s_addr =
					bootpreply->bp_yiaddr.s_addr;
#ifndef	NO_DHCP_SUPPORT
				dhcp_addr.s_addr = bootpreply->bp_yiaddr.s_addr;
#endif	/* NO_DHCP_SUPPORT */
				netmask = default_netmask();
				arptable[ARP_SERVER].ipaddr.s_addr =
					bootpreply->bp_siaddr.s_addr;
				memset(arptable[ARP_SERVER].node, 0, ETH_ALEN);  /* Kill arp */
				arptable[ARP_GATEWAY].ipaddr.s_addr =
					bootpreply->bp_giaddr.s_addr;
				memset(arptable[ARP_GATEWAY].node, 0, ETH_ALEN);  /* Kill arp */
				/* bootpreply->bp_file will be copied to KERNEL_BUF in the memcpy */
				memcpy((char *)BOOTP_DATA_ADDR, (char *)bootpreply, sizeof(struct bootpd_t));
				decode_rfc1533(BOOTP_DATA_ADDR->bootp_reply.bp_vend,
#ifdef	NO_DHCP_SUPPORT
				       0, BOOTP_VENDOR_LEN + MAX_BOOTP_EXTLEN, 1);
#else
				       0, DHCP_OPT_LEN + MAX_BOOTP_EXTLEN, 1);
#endif	/* NO_DHCP_SUPPORT */
#ifdef	REQUIRE_VCI_ETHERBOOT
				if (!vci_etherboot)
					return (0);
#endif
				return(1);
			}

#ifdef	DOWNLOAD_PROTO_TFTP
					/* TFTP ? */
			if ((type == AWAIT_TFTP) &&
				(ntohs(udp->dest) == ival)) return(1);
#endif	/* DOWNLOAD_PROTO_TFTP */

#ifdef	DOWNLOAD_PROTO_NFS
					/* RPC ? */
			rpc = (struct rpc_t *)&nic.packet[ETH_HLEN];
			if ((type == AWAIT_RPC) &&
			    (ntohs(udp->dest) == ival) &&
			    (*(unsigned long *)ptr == ntohl(rpc->u.reply.id)) &&
			    (ntohl(rpc->u.reply.type) == MSG_REPLY)) {
				return (1);
			}
#endif	/* DOWNLOAD_PROTO_NFS */

		} else {
			/* Check for abort key only if the Rx queue is empty -
			 * as long as we have something to process, don't
			 * assume that something failed.  It is unlikely that
			 * we have no processing time left between packets.  */
			if (iskey() && (getchar() == ESC))
				longjmp(restart_etherboot, -1);
			/* Do the timeout after at least a full queue walk.  */
			if ((timeout == 0) || (currticks() > time)) {
				break;
			}
		}
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
int decode_rfc1533(unsigned char *p, int block, int len, int eof)
{
	static unsigned char *extdata = NULL, *extend = NULL;
	unsigned char        *extpath = NULL;
	unsigned char        *endp;

#ifdef	REQUIRE_VCI_ETHERBOOT
	vci_etherboot = 0;
#endif
	if (block == 0) {
#ifdef	IMAGE_MENU
		memset(imagelist, 0, sizeof(imagelist));
		menudefault = useimagemenu = 0;
		menutmo = -1;
#endif
#ifdef	MOTD
		memset(motd, 0, sizeof(motd));
#endif
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
		return (-1);
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
		else if (c == RFC1533_NETMASK)
			memcpy(&netmask, p+2, sizeof(in_addr));
		else if (c == RFC1533_GATEWAY) {
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
		else if (c == RFC1533_VENDOR) {
			vci_etherboot = find_vci_etherboot(p+1);
#ifdef	MDEBUG
			printf("vci_etherboot %d\n", vci_etherboot);
#endif
		}
#endif	/* REQUIRE_VCI_ETHERBOOT */
		else if (c == RFC2132_MSG_TYPE)
			dhcp_reply=*(p+2);
		else if (c == RFC2132_SRV_ID)
			memcpy(&dhcp_server, p+2, sizeof(in_addr));
#endif	/* NO_DHCP_SUPPORT */
		else if (c == RFC1533_HOSTNAME) {
			hostname = p + 2;
			hostnamelen = *(p + 1);
		}
		else if (c == RFC1533_VENDOR_MAGIC
			 && TAG_LEN(p) >= 6 &&
			  !memcmp(p+2,vendorext_magic,4) &&
			  p[6] == RFC1533_VENDOR_MAJOR
			)
			vendorext_isvalid++;
#ifdef	IMAGE_FREEBSD
		else if (c == RFC1533_VENDOR_HOWTO)
			freebsd_howto = ((p[2]*256+p[3])*256+p[4])*256+p[5];
		else if (c == RFC1533_VENDOR_KERNEL_ENV){
			if(*(p + 1) < sizeof(freebsd_kernel_env)){
				memcpy(freebsd_kernel_env,p+2,*(p+1));
			}else{
				printf("Only support %d bytes in Kernel Env %d\n",sizeof(freebsd_kernel_env));
			}
		}
#endif
#ifdef	IMAGE_MENU
		else if (c == RFC1533_VENDOR_MNUOPTS)
			parse_menuopts(p+2, TAG_LEN(p));
		else if (c >= RFC1533_VENDOR_IMG &&
			 c<RFC1533_VENDOR_IMG+RFC1533_VENDOR_NUMOFIMG) {
			imagelist[c - RFC1533_VENDOR_IMG] = p;
			useimagemenu++;
		}
#endif
#ifdef	MOTD
		else if (c >= RFC1533_VENDOR_MOTD &&
			 c < RFC1533_VENDOR_MOTD +
			 RFC1533_VENDOR_NUMOFMOTD)
			motd[c - RFC1533_VENDOR_MOTD] = p;
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
	if (block == 0 && extpath != NULL) {
		char fname[64];
		memcpy(fname, extpath+2, TAG_LEN(extpath));
		fname[(int)TAG_LEN(extpath)] = '\0';
		printf("Loading BOOTP-extension file: %s\n",fname);
		download(fname, decode_rfc1533);
	}
	return (-1);	/* proceed with next block */
}

/**************************************************************************
IPCHKSUM - Checksum IP Header
**************************************************************************/
static unsigned short ipchksum(unsigned short *ip, int len)
{
	unsigned long sum = 0;
	len >>= 1;
	while (len--) {
		sum += *(ip++);
		if (sum > 0xFFFF)
			sum -= 0xFFFF;
	}
	return((~sum) & 0x0000FFFF);
}

#define TWO_SECOND_DIVISOR (2147483647l/TICKS_PER_SEC)

/**************************************************************************
RFC2131_SLEEP_INTERVAL - sleep for expotentially longer times
**************************************************************************/
long rfc2131_sleep_interval(int base, int exp)
{
	static long seed = 0;
	long q;
	unsigned long tmo;

#ifdef BACKOFF_LIMIT
	if (exp > BACKOFF_LIMIT)
		exp = BACKOFF_LIMIT;
#endif
	if (!seed) /* Initialize linear congruential generator */
		seed = currticks() + *(long *)&arptable[ARP_CLIENT].node
		       + ((short *)arptable[ARP_CLIENT].node)[2];
	/* simplified version of the LCG given in Bruce Schneier's
	   "Applied Cryptography" */
	q = seed/53668;
	if ((seed = 40014*(seed-53668*q) - 12211*q) < 0) seed += 2147483563L;
	tmo = (base << exp) + (TICKS_PER_SEC - (seed /TWO_SECOND_DIVISOR));
	return tmo;
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
#if	defined(ANSIESC) && defined(CONSOLE_CRT)
	ansi_reset();
#endif
}

/*
 * Local variables:
 *  c-basic-offset: 8
 * End:
 */
