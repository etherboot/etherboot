/**************************************************************************
*
*    dns_resolver.c: Etherboot support for resolution of host/domain
*    names in filename parameters
*    Written 2004 by Anselm M. Hoffmeister
*    <stockholm@users.sourceforge.net>
*
*    This program is free software; you can redistribute it and/or modify
*    it under the terms of the GNU General Public License as published by
*    the Free Software Foundation; either version 2 of the License, or
*    (at your option) any later version.
*
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU General Public License for more details.
*
*    You should have received a copy of the GNU General Public License
*    along with this program; if not, write to the Free Software
*    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*
*    This code is using nuts and bolts from throughout etherboot.
*    It is a fresh implementation according to the DNS RFC, # TODO
*    
*    $Revision$
*    $Author$
*    $Date$
*
*    REVISION HISTORY:
*    ================
*    2004-05-10 File created
***************************************************************************/

#ifdef DNS_RESOLVER
#warning ========RESOLVER IS EXPERIMENTAL ONLY========
#warning ========USE WITH CAUTION, CODE IS KNOWN======
#warning ========AND EXPECTED TO BE BUGGY NOW=========
#include "etherboot.h"
#include "nic.h"
#define	MAX_DNS_RETRIES	3

int	donameresolution ( char * hostname, int hnlength, char * deststring );

/*
 *	dns_resolver
 *	Param:	string filename
 *	Return:	string filename, with hostname replaced by IP in dotted quad
 *		or NULL for resolver error
 *	In case there is no substitution necessary, return will be = param
 */
char *	dns_resolver ( char * filename ) {
	int	i = 0, j;
	char	ipaddr[16] = { 0 };
	// Hostname found, search for end of hostname
	// TODO: All-numeric hostnames need not to be checked, but CHECK if they
	// will be converted when this function returns (somewhere in nic.c!?)
	for ( j = i; (filename[j] != ':') && (filename[j] != '/'); ++j ) {
		if ( filename[j] == 0 )	return	filename;
	}
	if ( donameresolution ( filename + i, j - i, ipaddr ) ) {
		return	NULL;
	}
	// TODO: stick the ipaddr into the filename
	return	ipaddr;
}

/*
 *	await_dns
 *	Param:	as any await functions
 *	Return:	0 for is_not_our_packet, 1 for Adr_returned, 2 for needs_CNAME, 3 for
 *	needs_A_after_CNAME, 4 for CNAME_failed, 5 for name_does_not_exist
 */
static int await_dns (int ival, void *ptr,
	unsigned short ptype, struct iphdr *ip, struct udphdr *udp,
	struct tcphdr *tcp __unused) {
	int	i;
	unsigned char *p = (unsigned char *)udp + sizeof(struct udphdr);
	unsigned char *q;
	if ( 0 == udp ) return 0;
	if ( 53 != ntohs (udp->src) ) return 0;
	if ( 53 != ntohs (udp->dest) ) return 0;
	if ( p[0] != (ival & 0xff ) ) return 0; // Not matching
	if ( p[1] != 0 ) return 0;		// our request ID
	if ( 0 == ( p[2] & 0x80 ) ) return 0;	// Is not a response
	if ( 0 != ( p[2] & 0x78 ) ) return 0;	// Is not a standard query
	if ( (0 == ( p[6] + p[7] ) ) || // Answer section has 0 entries
	     ( 3 == ( p[3] & 0x0f ) ) ) { // Error: "NO SUCH NAME"
			if ( 0x100 == ( ival & 0xf00 ) ) {
			// Was an A query
			return	2; // So try CNAME query next
		} else if ( 0x500 == ( ival & 0xf00 ) ) {
			return	5; // Was a CNAME, unsuccesful -> error
		} else {
			// Anything else? Error in calling await_dns, so...
			return	5; // Bail out with error
		}
	}
	if ( 0 != ( p[3] & 0x0f ) ) return 4;	// Another error occured
	// Now we have an answer, but we must hunt it first, as there is a question
	// section first usually
	// If question section was not repeated, that saves a lot of work :-)
	if ( 0 >= ( i = ( ( p[4] * 0x100 ) + p[5] ) ) ) {
		q = p + 12; // No question section, we can just continue;
	} else if ( i >= 2 ) {
		// More than one query section? Error!
		return	4;
	} else {
		// We have to skip through the question section first to
		// find the beginning of the answer section
		q = p + 12;
		while ( 0 != q[0] ) q += q[0] + 1; // Skip through
		q+=5; // Skip over end-\0 and query type section
	}
	// Now our pointer shows the beginning of the answer section
	while ( 0 != q[0] ) {
		if ( 0xc0 == ( q[0] & 0xc0 ) ) { // Pointer
			++q;
			break;
		}
		q += q[0] + 1;
	}
	++q;
	// Now check wether it's an INET host address (resp. CNAME)?
	if ( (0 != q[0]) || ( (ival & 0xf00) != (q[1] * 0x100) ) ||
			(0 != q[2]) || (1 != q[3]) ) {
		// Doesn't match our query
		return	4;
	}
	q += 8; // skip ttl
	if ( (ival & 0xf00) == 0x100 ) { // A QUERY
		p = ptr+256;
		//printf ( "DNS resolved to address [");
		for ( i = 0; i < 4; ++i ) {
			p[i] = q[i+2];
			//printf ("%s%d", (i?".":""),p[i]);
		}
		//printf ("]\n");
		return	1; // OK! A RETURNED! VERY FINE!
	}
	// Must be a CNAME query then.
	// TODO: Interpret CNAME results.
		
	//printf ("Received answer: [");
	//for ( i = sizeof(struct udphdr) ; (i < 64) && (i < ntohs(udp->len)); ++i ) {
	//	printf ( "%x ", ((unsigned char *)udp)[i] );
	//}
	//printf ( "]\n" );
	return	4;
}

int	chars_to_next_dot ( char * countfrom, int maxnum ) {
	int i;
	for ( i = 1; i < maxnum; ++i ) { // On purpose, omit current
		if ( countfrom[i] == '.' ) return i;
	}
	return	maxnum;
}

/*
 *	donameresolution
 *	Param:	string hostname, length (hostname needs no \0-end-marker),
 *		string to which dotted-quad-IP shall be written
 *	Return:	0 for success, >0 for failure
 */
int	donameresolution ( char * hostname, int hnlength, char * deststring ) {
	char	querybuf[260+sizeof(struct iphdr)+sizeof(struct udphdr)];
		// 256 for the DNS query packet, +4 for the result temporary
	char	*query = &querybuf[sizeof(struct iphdr)+sizeof(struct udphdr)];
	int	i;
	long	timeout;
	int	retry;
	//struct	iphdr * iph = querybuf;
	//struct	udphdr* udph= querybuf + sizeof(struct iphdr);
	query[0] = 1; // Arbitrary numbers,
	query[1] = 0; // "1" will do :-) -- this is a session number thing
	query[2] = query[3] = 0; query[5] = 1;
	query[4] = query[6] = query[7] = query[8] = query[9] = query[10] = query[11] = 0;
	//{ 47,11, 0,0, 1,0, 0,0, 0,0, 0,0 } ;
	query[12] = chars_to_next_dot(hostname,hnlength); //hnlength + 1;
	if ( hnlength > 236 ) return 1;
	for ( i = 0; i < hnlength; ++i ) {
		query[i+13] = hostname[i];
		if ( hostname[i] == '.' )
			query[i+13] = chars_to_next_dot(hostname + i + 1, hnlength - i - 1);
	}
	query[13+hnlength] = 0;
	query[14+hnlength] = 0; // 1 = A (TODO 5=CNAME)
	query[15+hnlength] = 1;
	query[16+hnlength] = 0; // 1 = IP
	query[17+hnlength] = 1;
	rx_qdrain(); // Clear NIC packet buffer - there won't be anything of interest
	udp_transmit ( arptable[ARP_NAMESERVER].ipaddr.s_addr, (53),
			(53), hnlength + 18 + sizeof(struct iphdr)+sizeof(struct udphdr), querybuf );
	
	//printf ( "donameresolution [%s,%d]\n", hostname, hnlength );
	printf ( ".DNS." );
	for (retry = 1; retry <= MAX_DNS_RETRIES; retry++) {
		timeout = rfc2131_sleep_interval(TIMEOUT, retry);
		i = await_reply(await_dns, 0x100 + ((unsigned char *)query)[0], query, timeout);
		// query + 256 points to the place where A records shall go;
		// query + 12 points to where a CNAME should go
		// the 0x100 means 1 = A query running
		if (i) break;
		printf ( ".DNS.");
	}
	// TODO: circulate for CNAME queries etc.
	//printf ( "Await reply gave %d...", i );
	if ( i == 1 ) {
		printf ( "resolved to [%d.%d.%d.%d]\n", query[256],query[257],query[258],query[259] );
		sprintf( deststring, "%d.%d.%d.%d", query[256], query[257], query[258], query[259] );
		return	0;
	}
	else {
		printf ( "Name resolution failed\n" );
	}
	return	1; // not implemented yet
}
#endif				/* DNS_RESOLVER */
