#ifdef DOWNLOAD_PROTO_TFTM
#include "etherboot.h"
#include "nic.h"

#define TFTM_PORT 1758
#define BLOCKSIZE 1024

struct tftm_info {
	in_addr  server_ip;
	in_addr  multicast_ip;
	in_addr  local_ip;
	uint16_t server_port;
	uint16_t multicast_port;
	uint16_t local_port;
	uint16_t numblocks;
	int (*fnc)(unsigned char *, unsigned int, unsigned int, int);
	char ismaster;
	char *   data;
	uint16_t datalen;
	char yet;
};

#define	TFTM_GARBAGE	0
#define	TFTM_REPLY	1
#define	TFTM_DATA	2


static int await_tftm_packet(int ival, void *ptr,
	unsigned short ptye, struct iphdr *ip, struct udphdr *udp)
{	/* To be implemented. What does this thing do anyway? */
	struct tftm_info *info = ptr;
	int	i;
	unsigned char	c;
	if (!udp) return TFTM_GARBAGE;
	/*
	 * What is expected here?
	 * 1/ unicast from tftm-server (switch-to-master etc.)
	 * 2/ multicast from tftm-server (data, nothing else)
	 * 3/ multicast garbage, as hardware filtering is always
	 *    imperfect, if enabled at all - for now, it even isn't.
	 */
	if (ip->dest.s_addr == arptable[ARP_CLIENT].ipaddr.s_addr)
	{ /* To our unicast address */
		if ( udp->dest == info->local_port )
		{ /* Even to our port. Must be something worthy! */
			if ( nic.packetlen > ETH_HLEN +
			 sizeof(struct iphdr)+sizeof(struct udphdr) )
			{
				info->data = nic.packet + ETH_HLEN + sizeof ( struct iphdr ) + sizeof ( struct udphdr );
				info->datalen = nic.packetlen - ETH_HLEN - sizeof ( struct iphdr ) - sizeof ( struct udphdr );
				return TFTM_REPLY;
			}
		}
		else return TFTM_GARBAGE;
	}
	if ((ip->dest.s_addr  == info->multicast_ip.s_addr) &&
	    (ntohs(udp->dest) == info->multicast_port) &&
	    (nic.packetlen >= ETH_HLEN + sizeof(struct iphdr) +
	    sizeof ( struct udphdr ))) {
		info->data = nic.packet + ETH_HLEN + sizeof ( struct iphdr ) + sizeof ( struct udphdr );
		info->datalen = nic.packetlen - ETH_HLEN - sizeof ( struct iphdr ) - sizeof ( struct udphdr );
		return TFTM_DATA;
	}
	return	TFTM_GARBAGE;
}


int url_tftm(const char *name, int (*fnc)(unsigned char *, unsigned int, unsigned int, int))
{
	struct tftp_t *tr;
	struct tftpreq_t tp;
	unsigned short len;
	int retry = 0;
	unsigned short i = 0, j = 0;
	long timeout;
	unsigned long filesize;
	struct tftm_info info;
	unsigned char * p1, * p2, * p3;
	unsigned char	a[4];
	unsigned char *	memlocation = 0;
	unsigned short bord1 = 0, bord2 = 0, bord3 = 0, bord4 = 0;
	unsigned short totalblocks = 0, loadedblocks = 0;
	unsigned long tttt1 = 0;
	/* Set the defaults */
	info.server_ip.s_addr    = arptable[ARP_SERVER].ipaddr.s_addr;
	info.server_port         = TFTM_PORT;
	info.local_ip.s_addr     = arptable[ARP_CLIENT].ipaddr.s_addr;
	info.local_port          = TFTM_PORT; /* Does not matter. So take tftm port too. */
	info.multicast_ip.s_addr = info.local_ip.s_addr;
	info.multicast_port      = TFTM_PORT;
	info.fnc                 = fnc;
	info.ismaster		 = 0;
	info.yet		 = 0;
	info.data		 = "@";
	info.datalen		 = 2;
	if (name[0] != '/') {
		/* server ip given, so use it */
		name += inet_aton(name, &info.server_ip);
		/* No way to specify another port for now */
	}
	if (name[0] != '/') {
		printf ( "Bad tftm-URI: [%s]\n", name );
		return 0;
	}
	arptable[ARP_TFTM].ipaddr.s_addr = info.server_ip.s_addr;
	/* Send a tftm-request to the server */
	tp.opcode = 256; /* Const for "\0x0" "\0x1" =^= ReadReQuest */
	len = sizeof ( tp.ip) + sizeof ( tp.udp ) + sizeof ( tp.opcode ) + sprintf ( (char*)tp.u.rrq,
	 "%s%coctet%cmulticast%c%cblksize%c%d%ctsize%c", name, 0, 0, 0, 0, 0, BLOCKSIZE, 0, 0 ) + 1;
	rx_qdrain();	/* Get the receive queue clean here */
	if ( !udp_transmit ( arptable[ARP_TFTM].ipaddr.s_addr,
	 htons(info.local_port), info.server_port, len, &tp ) )
	{
		printf ( "Could not send TFTM-Request to server\n" ); return 0;
	}
	/* Await multicast confirmation from server. Must be addressed
	 *   to our unicast ip, local_port. 
	 */
	timeout = rfc2131_sleep_interval ( TIMEOUT, retry );
	for ( i = 0; i == 0 ; ) {
		/* Do some kind of timeout valuing here: */
		if ( 0 != (i = await_reply(await_tftm_packet, htons(info.local_port), (void *)&info, timeout) ) ) {
			/* Now parse that data block if it really is a
			 * multicast confirmation and find the
			 * ip-address for multicast mode. And port number.
			 */
			if ( ( *info.data != 0 ) || ( *(info.data + 1) != 6 ) || ( info.datalen < 18 ) ) {
				printf ( "TFTM-Server rejects multicast. Strange. <Abort>\n" );
				/* __TODO__ implement IGMP for making
				 * any routing and switching device
				 * know that we NEED that packet!
				 */
				return	0;
			}
			i = j = 0;
			for ( p1 = info.data + 2; (void *)p1 < (void *) (info.data + info.datalen - 10); ++p1 ) {
				for ( p2 = p1; ( (void *)p2 < (void *) (info.data + info.datalen - 1) ) && ( *p2 != 0 ); ++p2 ) ;
				if ( *p2 != 0 ) { *(p2 = info.data + info.datalen - 1) = 0; } else { ++p2; }
				/* Assure there is no buffer overrun,
				 * so get the end of "p2" at the same
				 * time: 
				 */
				for ( p3 = p2; ( (void *)p3 < (void *) (info.data + info.datalen) ) && ( *p3 != 0 ); ++p3 ) ;
				if ( *p3 != 0 ) { *(p3 = info.data + info.datalen - 1) = 0; }
				if ( ! strncmp ( p1, "multicast", 10 ) ) {
					i |= 1;
				}
				if ( ! strncmp ( p1, "blksize", 8 ) ) {
					i |= 2;
					j = strtoul(p2, &p2, 10);
					if ( j != BLOCKSIZE ) {
						printf ( "TFTM-Server rejected required transfer blocksize %d\n", 
							BLOCKSIZE );
						return	0;
					}
				}
				if ( ! strncmp ( p1, "tsize", 6 ) ) {
					if ( ( filesize = strtoul(p2, &p2, 10) ) > 0 ) i |= 4;
					if ( filesize > ( meminfo.memsize << 10 ) ) {
						printf ( "File is too large (%ld k) - only %d k memory available!\n",
						 ( filesize & 0xfffffc00 ) >> 10, meminfo.memsize );
						return	0;
					}
					info.numblocks = 1 + ( filesize - ( filesize % BLOCKSIZE ) ) / BLOCKSIZE;
					if ( info.numblocks > 0xffff ) {
						printf ( "File is too large (%ld k) - at blocksize %d max. %d k (%d blocks) can be transferred\n",
						 ( filesize & 0xfffffc00 ) >> 10, BLOCKSIZE, ( ( 0xffff * BLOCKSIZE ) & 0xfffffc00 ) >> 10, 0xffff );
						return	0;
					}
					totalblocks = info.numblocks;
					memlocation = (unsigned char *)(( 1024 * 1024 ) + (meminfo.memsize << 10)) - ( totalblocks * BLOCKSIZE );
				}
				p1 = p3;
			}
			if ( i != 7 ) {
				printf ( "TFTM-Server doesn't understand options [blksize tsize multicast]\n" );
				return	0;
			}
			p1 = info.data + 12;
			p1 += 1 + inet_aton ( p1, &info.multicast_ip );
			info.multicast_port = strtoul(p1, &p1, 10); ++p1;
			info.ismaster = ( *p1 == '1' ? 1 : 0 );
			i = 1;
		}
		else
		{
			if ( retry++ < MAX_TFTP_RETRIES ) {
#ifndef SILENTFORSPLASH
				printf ( "Retransmit TFTM request...\n" );
#endif 
				if ( !udp_transmit ( arptable[ARP_TFTM].ipaddr.s_addr,
				 htons(info.local_port), info.server_port, len, &tp ) ) {
					printf ( "Could not send TFTM request\n" );
					return	0;
				}
				continue;
			}
			printf ( "TFTM Request timed out.\n" );
			return	0;
		}
	}
	retry = 0;
	info.yet = 1;
	for ( ;; ) {
		// If we are masterclient, request.
		if ( info.ismaster ) {
			tp.opcode = 1024; // Const for "0 4" =^= DataRequested
			len = sizeof ( tp.ip) + sizeof ( tp.udp ) + sizeof ( tp.opcode ) + 2;
			tp.u.ack.block = htons ( bord1 );
			if ( !udp_transmit ( arptable[ARP_TFTM].ipaddr.s_addr,
			 htons(info.local_port), info.server_port, len, &tp ) ) {
				printf ( "Could not sent packet to TFTM-Server\n" );
				return	0;
			}
		}
		if ( TFTM_GARBAGE == ( i = await_reply(await_tftm_packet, htons(info.local_port), (void *)&info, timeout ) ) ) {
			if ( retry++ < MAX_TFTP_RETRIES ) {
				continue;
			}
			printf ( "Timeout on Multicast connection\n" ); return 0;
		}
		else if ( i == TFTM_REPLY ) {
			// Get a unicast packet
			// It should notify us if we now are masterclient or not.
			if ( ( *info.data != 0 ) || ( *(info.data + 1) != 6 ) || ( info.datalen < 2 ) ) {
				printf ( "TFTM-Server won't run multicast. Strange.\n<Abort>\n" );
				return	0;
			}
			if ( strncmp ( info.data + 2, "multicast\0,,", 12 ) ) {
				printf ( "TFTM-Server doesn't know wether it knows multicast....!?\n" );
				return	0;
			}
			p1 = info.data + 14;
			info.ismaster = ( *p1 == '1' ? 1 : 1 );
		}
		else if ( i == TFTM_DATA ) {
			// Get a multicast packet
			p1 = info.data;
			/* p1 += 12; */
			if ( ( 0 == *p1 ) && ( 3 == *(p1+1) ) ) {
				p1+=2;
				i = ( ( *p1 ) << 8 ) + *(p1+1);
				/* See if we can use the blocknumber. */
				j = 1;
				if ( info.datalen < BLOCKSIZE + 4 )
				{
					bord4 = i;
				}
				if ( i == bord1 + 1 ) {
					++bord1;
					if ( bord1 + 1 == bord2 ) {
						bord1 = bord3;
						bord2 = bord3 = 0;
					}
				}
				else if ( i == bord3 + 1 ) {
					++bord3;
				}
				else if ( bord3 == 0 ) {
					bord3 = bord2 = i;
				}
				else {
					// No use for that packet
					j = 0;
				}
				if ( ( j != 0 ) && ( totalblocks >= i ) ) {
					loadedblocks++;
#ifndef SILENTFORSPLASH
#ifdef SIZEINDICATOR
					printf  ( "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b " );
					putchar ( '0' + (loadedblocks/1000)%10);
					putchar ( '0' + (loadedblocks/100 )%10);
					putchar ( '0' + (loadedblocks/10  )%10);
					putchar ( '0' + (loadedblocks/1   )%10);
					putchar ( '/' );
					putchar ( '0' + ( totalblocks/1000)%10);
					putchar ( '0' + ( totalblocks/100 )%10);
					putchar ( '0' + ( totalblocks/10  )%10);
					putchar ( '0' + ( totalblocks/1   )%10);
					printf  ( " blocks" );
#endif
#endif
				/* Put this packet into RAM **TODO** */
					memcpy ( (unsigned char *)(memlocation + (i - 1) * BLOCKSIZE), info.data + 4, BLOCKSIZE );
					
				}
				/* Test for is_last_packet!!!! */
				if ( ( info.ismaster ) && ( i == bord4 ) )
				{  
				/* Gracefully say goodbye... We are loaded! */
#ifndef SILENTFORSPLASH
					printf ( "\n\n===========================================================================\nFinished loading %d packets!\n", bord4 );
#endif
					return	fnc(memlocation, 1, filesize, 1);
				}
			}
			
		}
		else {
			/* printf ( "Sorry, got garbage from the network\n" ); */
		}
	}
	return	0;
}
#endif /* DOWNLOAD_PROTO_TFTM */
