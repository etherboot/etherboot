/*****************************************************************************
 *
 * wol.c - Wake-On-LAN utility to wake a networked PC
 *
 * by R. Edwards (bob@cs.anu.edu.au), January 2000
 * (in_ether routine adapted from net-tools-1.51/lib/ether.c by
 * Fred N. van Kempen)
 * added file input, some minor changes for compiling for NetWare
 * by G. Knauf (gk@gknw.de), 12-Okt-2000
 *
 * This utility allows a PC with WOL configured to be powered on by
 * sending a "Magic Packet" to it's network adaptor (see:
 * http://www.amd.com/products/npd/overview/20212.html).
 * Only the ethernet MAC address needs to be given to make this work.
 *
 * Current version uses a UDP broadcast to send out the Magic Packet.
 * A future version will use the raw packet interface to unicast the
 * packet only to the target machine.
 *
 * compile with: gcc -Wall -o wol wol.c
 *
 * usage: wol <MAC address>
 * where <MAC address> is in xx:xx:xx:xx:xx:xx format.
 * or: wol -f <File name>  
 * where <File name> is a file containing one MAC address per line,
 * optional followed by a hostname or ip separated by a blank.
 *
 * Released under GNU Public License January, 2000.
 */

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

static int	read_file (char *macfile);
static int	in_ether (char *bufp, unsigned char *addr);
static int	send_wol (char *mac, char *host);

char		*Program;


int main (int argc, char *argv[])
{
    Program = argv[0];

    if (argc < 2) {
    	fprintf (stderr, "\r%s: need hardware address or file option\n", Program);
    	return (-1);
    }
    if (!strcmp (argv[1], "-f")) {
  	if (!argv[2])
    	    fprintf (stderr, "\r%s: need argument for file option\n", Program);
        else
    	    read_file (argv[2]);
    } else
 	send_wol (argv[1], "");
	return (0);
}


static int in_ether (char *bufp, unsigned char *addr)
{
    char c, *orig;
    int i;
    unsigned char *ptr = addr;
    unsigned val;

    i = 0;
    orig = bufp;
    while ((*bufp != '\0') && (i < 6)) {
        val = 0;
        c = *bufp++;
        if (isdigit(c))
            val = c - '0';
        else if (c >= 'a' && c <= 'f')
            val = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F')
            val = c - 'A' + 10;
        else {
#ifdef DEBUG
            fprintf(stderr, "\rin_ether(%s): invalid ether address!\n", orig);
#endif
            errno = EINVAL;
            return (-1);
        }
        val <<= 4;
        c = *bufp;
        if (isdigit(c))
            val |= c - '0';
        else if (c >= 'a' && c <= 'f')
            val |= c - 'a' + 10;
        else if (c >= 'A' && c <= 'F')
            val |= c - 'A' + 10;
        else if (c == ':' || c == 0)
            val >>= 4;
        else {
#ifdef DEBUG
            fprintf(stderr, "\rin_ether(%s): invalid ether address!\n", orig);
#endif
            errno = EINVAL;
            return (-1);
        }
        if (c != 0)
            bufp++;
        *ptr++ = (unsigned char) (val & 0377);
        i++;

        /* We might get a semicolon here - not required. */
        if (*bufp == ':') {
            if (i == 6) {
                    ;           /* nothing */
            }
            bufp++;
        }
    }
	if (bufp - orig != 17) {
        return (-1);
    } else {
        return (0);
    }
} /* in_ether */


static int read_file (char *macfile)
{
	FILE	*pfile = NULL;
	char	mac[32];
	char	host[32];
	char	buffer[512];

	pfile = fopen (macfile, "r+");

	if (pfile) {
		while (fgets (buffer, 511, pfile) != NULL) {
			if (buffer[0] != '#' && buffer[0] != ';') {
				mac[0] = host[0] = '\0';
		 		sscanf (buffer, "%s %s", mac, host);
		 		send_wol (mac, host);
		 	}

		}
		fclose (pfile);
		return (0);
	} else {
    		fprintf (stderr, "\r%s: macfile '%s' not found\n", Program, macfile);
  		return (-1);
	}
}


static int send_wol (char *mac, char *host)
{
        int i, j;
        int packet;
        struct sockaddr_in sap;
        unsigned char ethaddr[8];
        unsigned char *ptr;
        unsigned char buf [128];
        int optval = 1;

        /* Fetch the hardware address. */
        if (in_ether (mac, ethaddr) < 0) {
            fprintf (stderr, "\r%s: invalid hardware address\n", Program);
            return (-1);
        }

        /* setup the packet socket */
        if ((packet = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
                fprintf (stderr, "\r%s: socket failed\n", Program);
                return (-1);
        }

        /* Set socket options */
        if (setsockopt (packet, SOL_SOCKET, SO_BROADCAST, (char *)&optval,
                sizeof (optval)) < 0) {
                fprintf (stderr, "\r%s: setsocket failed %s\n", Program, strerror (errno));
                close (packet);
                return (-1);
        }

	/* Set up broadcast address */
        sap.sin_family = AF_INET;
        sap.sin_addr.s_addr = htonl(0xffffffff);        /* broadcast address */
        sap.sin_port = htons(60000);

        /* Build the message to send - 6 x 0xff then 16 x MAC address */
        ptr = buf;
        for (i = 0; i < 6; i++)
                *ptr++ = 0xff;
        for (j = 0; j < 16; j++)
                for (i = 0; i < 6; i++)
                        *ptr++ = ethaddr [i];

        /* Send the packet out */
        if (sendto (packet, (char *)buf, 102, 0, (struct sockaddr *)&sap, sizeof (sap)) < 0) {
                fprintf (stderr, "\r%s: sendto failed, %s\n", Program, strerror(errno));
                close (packet);
            return (-1);
        }
	close (packet);
	fprintf (stderr, "\r%s: magic packet sent to %s %s\n", Program, mac, host);

  return (0);
}
