/*
* 3c595.c -- 3COM 3C595 Fast Etherlink III PCI driver for etherboot
*
* Copyright (C) 2000 Shusuke Nisiyama <shu@athena.qe.eng.hokudai.ac.jp>
* All rights reserved.
* Mar. 14, 2000
*
*  This software may be used, modified, copied, distributed, and sold, in
*  both source and binary form provided that the above copyright and these
*  terms are retained. Under no circumstances are the authors responsible for
*  the proper functioning of this software, nor do the authors assume any
*  responsibility for damages incurred with its use.
*
* This code is based on Martin Renters' etherboot-4.4.3 3c509.c and 
* Herb Peyerl's FreeBSD 3.4-RELEASE if_vx.c driver.
*
*  Copyright (C) 1993-1994, David Greenman, Martin Renters.
*  Copyright (C) 1993-1995, Andres Vega Garcia.
*  Copyright (C) 1995, Serge Babkin.
*
*  Copyright (c) 1994 Herb Peyerl <hpeyerl@novatel.ca>
*
*/

/* #define EDEBUG */

#include "etherboot.h"
#include "nic.h"
#include "pci.h"
#include "3c595.h"

static unsigned char	eth_vendor, eth_flags, eth_laar;
static unsigned short	eth_nic_base, eth_asic_base;
static unsigned short	vx_connector, vx_connectors;

static struct connector_entry {
  int bit;
  char *name;
} conn_tab[VX_CONNECTORS] = {
#define CONNECTOR_UTP   0
  { 0x08, "utp"},
#define CONNECTOR_AUI   1
  { 0x20, "aui"},
/* dummy */
  { 0, "???"},
#define CONNECTOR_BNC   3
  { 0x10, "bnc"},
#define CONNECTOR_TX    4
  { 0x02, "tx"},
#define CONNECTOR_FX    5
  { 0x04, "fx"},
#define CONNECTOR_MII   6
  { 0x40, "mii"},
  { 0, "???"}
};

static void vxgetlink(void);
static void vxsetlink(void);

static void safetwiddle()
{
	static int count=0;
	static int count2=0;
	static char tiddles[]="-\\|/";
	putchar(tiddles[(count2++)&0xfff?count&3:(count++)&3]);
	putchar('\b');
}

/* a surrogate */

static void DELAY(int val)
{
	int c;

	for(c=0; c<val; c+=20) {
		twiddle();
	}
}

static void SAFEDELAY(int val) 
{
	int c;
	
	for (c=0; c<val; c+=20) {
		safetwiddle();
	}
}

/**************************************************************************
ETH_RESET - Reset adapter
***************************************************************************/
static void t595_reset(struct nic *nic)
{
	int i, j;

	/***********************************************************
			Reset 3Com 595 card
	*************************************************************/

	/* stop card */
	outw(RX_DISABLE, BASE + VX_COMMAND);
	outw(RX_DISCARD_TOP_PACK, BASE + VX_COMMAND);
	VX_BUSY_WAIT;
	outw(TX_DISABLE, BASE + VX_COMMAND);
	outw(STOP_TRANSCEIVER, BASE + VX_COMMAND);
	DELAY(8000);
	outw(RX_RESET, BASE + VX_COMMAND);
	VX_BUSY_WAIT;
	outw(TX_RESET, BASE + VX_COMMAND);
	VX_BUSY_WAIT;
	outw(C_INTR_LATCH, BASE + VX_COMMAND);
	outw(SET_RD_0_MASK, BASE + VX_COMMAND);
	outw(SET_INTR_MASK, BASE + VX_COMMAND);
	outw(SET_RX_FILTER, BASE + VX_COMMAND);

	/*
	* initialize card
	*/
	VX_BUSY_WAIT;

	GO_WINDOW(0);

	/* Disable the card */
/*	outw(0, BASE + VX_W0_CONFIG_CTRL); */

	/* Configure IRQ to none */
/*	outw(SET_IRQ(0), BASE + VX_W0_RESOURCE_CFG); */

	/* Enable the card */
/*	outw(ENABLE_DRQ_IRQ, BASE + VX_W0_CONFIG_CTRL); */

	GO_WINDOW(2);

	/* Reload the ether_addr. */
	for (i = 0; i < ETHER_ADDR_SIZE; i++)
		outb(nic->node_addr[i], BASE + VX_W2_ADDR_0 + i);

	outw(RX_RESET, BASE + VX_COMMAND);
	VX_BUSY_WAIT;
	outw(TX_RESET, BASE + VX_COMMAND);
	VX_BUSY_WAIT;

	/* Window 1 is operating window */
	GO_WINDOW(1);
	for (i = 0; i < 31; i++)
		inb(BASE + VX_W1_TX_STATUS);

	outw(SET_RD_0_MASK | S_CARD_FAILURE | S_RX_COMPLETE |
		S_TX_COMPLETE | S_TX_AVAIL, BASE + VX_COMMAND);
	outw(SET_INTR_MASK | S_CARD_FAILURE | S_RX_COMPLETE |
		S_TX_COMPLETE | S_TX_AVAIL, BASE + VX_COMMAND);

/*
 * Attempt to get rid of any stray interrupts that occured during
 * configuration.  On the i386 this isn't possible because one may
 * already be queued.  However, a single stray interrupt is
 * unimportant.
 */

	outw(ACK_INTR | 0xff, BASE + VX_COMMAND);

	outw(SET_RX_FILTER | FIL_INDIVIDUAL |
	    FIL_BRDCST, BASE + VX_COMMAND);

	vxsetlink();
/*{
	int i,j;
	i = CONNECTOR_TX;
	GO_WINDOW(3);
	j = inl(BASE + VX_W3_INTERNAL_CFG) & ~INTERNAL_CONNECTOR_MASK;
	outl(BASE + VX_W3_INTERNAL_CFG, j | (i <<INTERNAL_CONNECTOR_BITS));
        GO_WINDOW(4);
        outw(LINKBEAT_ENABLE, BASE + VX_W4_MEDIA_TYPE);
        GO_WINDOW(1);
}*/

	/* start tranciever and receiver */
	outw(RX_ENABLE, BASE + VX_COMMAND);
	outw(TX_ENABLE, BASE + VX_COMMAND);

}

/**************************************************************************
ETH_TRANSMIT - Transmit a frame
***************************************************************************/
static char padmap[] = {
	0, 3, 2, 1};

static void t595_transmit(
struct nic *nic,
const char *d,			/* Destination */
unsigned int t,			/* Type */
unsigned int s,			/* size */
const char *p)			/* Packet */
{
	register int len;
	int pad;
	int status;

#ifdef EDEBUG
	printf("{l=%d,t=%x}",s+ETHER_HDR_SIZE,t);
#endif

	/* swap bytes of type */
	t= htons(t);

	len=s+ETHER_HDR_SIZE; /* actual length of packet */
	pad = padmap[len & 3];

	/*
	* The 3c509 automatically pads short packets to minimum ethernet length,
	* but we drop packets that are too large. Perhaps we should truncate
	* them instead?
	*/
	if (len + pad > ETH_MAX_PACKET) {
		return;
	}

	/* drop acknowledgements */
	while(( status=inb(BASE + VX_W1_TX_STATUS) )& TXS_COMPLETE ) {
		if(status & (TXS_UNDERRUN|TXS_MAX_COLLISION|TXS_STATUS_OVERFLOW)) {
			outw(TX_RESET, BASE + VX_COMMAND);
			outw(TX_ENABLE, BASE + VX_COMMAND);
		}

		outb(0x0, BASE + VX_W1_TX_STATUS);
	}

	while (inw(BASE + VX_W1_FREE_TX) < len + pad + 4) {
		/* no room in FIFO */
	}

	outw(len, BASE + VX_W1_TX_PIO_WR_1);
	outw(0x0, BASE + VX_W1_TX_PIO_WR_1);	/* Second dword meaningless */

	/* write packet */
	outsw(BASE + VX_W1_TX_PIO_WR_1, d, ETHER_ADDR_SIZE/2);
	outsw(BASE + VX_W1_TX_PIO_WR_1, nic->node_addr, ETHER_ADDR_SIZE/2);
	outw(t, BASE + VX_W1_TX_PIO_WR_1);
	outsw(BASE + VX_W1_TX_PIO_WR_1, p, s / 2);
	if (s & 1)
		outb(*(p+s - 1), BASE + VX_W1_TX_PIO_WR_1);

	while (pad--)
		outb(0, BASE + VX_W1_TX_PIO_WR_1);	/* Padding */

	/* timeout after sending */
	DELAY(1000);
}

/**************************************************************************
ETH_POLL - Wait for a frame
***************************************************************************/
static int t595_poll(struct nic *nic)
{
	/* common variables */
	unsigned short type = 0;	/* used by EDEBUG */
	/* variables for 3C509 */
	short status, cst;
	register short rx_fifo;

	cst=inw(BASE + VX_STATUS);

#ifdef EDEBUG
	if(cst & 0x1FFF)
		printf("-%x-",cst);
#endif

	if( (cst & S_RX_COMPLETE)==0 ) {
		/* acknowledge  everything */
		outw(ACK_INTR | cst, BASE + VX_COMMAND);
		outw(C_INTR_LATCH, BASE + VX_COMMAND);

		return 0;
	}

	status = inw(BASE + VX_W1_RX_STATUS);
#ifdef EDEBUG
	printf("*%x*",status);
#endif

	if (status & ERR_RX) {
		outw(RX_DISCARD_TOP_PACK, BASE + VX_COMMAND);
		return 0;
	}

	rx_fifo = status & RX_BYTES_MASK;
	if (rx_fifo==0)
		return 0;

		/* read packet */
#ifdef EDEBUG
	printf("[l=%d",rx_fifo);
#endif
	insw(BASE + VX_W1_RX_PIO_RD_1, nic->packet, rx_fifo / 2);
	if(rx_fifo & 1)
		nic->packet[rx_fifo-1]=inb(BASE + VX_W1_RX_PIO_RD_1);
	nic->packetlen=rx_fifo;

	while(1) {
		status = inw(BASE + VX_W1_RX_STATUS);
#ifdef EDEBUG
		printf("*%x*",status);
#endif
		rx_fifo = status & RX_BYTES_MASK;

		if(rx_fifo>0) {
			insw(BASE + VX_W1_RX_PIO_RD_1, nic->packet+nic->packetlen, rx_fifo / 2);
			if(rx_fifo & 1)
				nic->packet[nic->packetlen+rx_fifo-1]=inb(BASE + VX_W1_RX_PIO_RD_1);
			nic->packetlen+=rx_fifo;
#ifdef EDEBUG
			printf("+%d",rx_fifo);
#endif
		}

		if(( status & RX_INCOMPLETE )==0) {
#ifdef EDEBUG
			printf("=%d",nic->packetlen);
#endif
			break;
		}

		DELAY(1000);
	}

	/* acknowledge reception of packet */
	outw(RX_DISCARD_TOP_PACK, BASE + VX_COMMAND);
	while (inw(BASE + VX_STATUS) & S_COMMAND_IN_PROGRESS);
#ifdef EDEBUG
	type = (nic->packet[12]<<8) | nic->packet[13];
	if(nic->packet[0]+nic->packet[1]+nic->packet[2]+nic->packet[3]+nic->packet[4]+
	    nic->packet[5] == 0xFF*ETHER_ADDR_SIZE)
		printf(",t=%#x,b]",type);
	else
		printf(",t=%#x]",type);
#endif
	return 1;
}


/*************************************************************************
	3Com 509 - specific routines
**************************************************************************/

static int
eeprom_rdy()
{
	int i;

	for (i = 0; is_eeprom_busy(BASE) && i < MAX_EEPROMBUSY; i++)
		DELAY(1000);
	if (i >= MAX_EEPROMBUSY) {
	        /* printf("3c509: eeprom failed to come ready.\n"); */
		printf("3c595: eeprom is busy.\n"); /* memory in EPROM is tight */
		return (0);
	}
	return (1);
}

/*
 * get_e: gets a 16 bits word from the EEPROM. we must have set the window
 * before
 */
static int
get_e(offset)
int offset;
{
	if (!eeprom_rdy())
		return (0xffff);
	outw(EEPROM_CMD_RD | offset, BASE + VX_W0_EEPROM_COMMAND);
	if (!eeprom_rdy())
		return (0xffff);
	return (inw(BASE + VX_W0_EEPROM_DATA));
}

static void            
vxgetlink(void)
{
    int n, k;

    GO_WINDOW(3);
    vx_connectors = inw(BASE + VX_W3_RESET_OPT) & 0x7f;
    for (n = 0, k = 0; k < VX_CONNECTORS; k++) {
      if (vx_connectors & conn_tab[k].bit) {
        if (n > 0) {
          printf("/");
	}
        printf(conn_tab[k].name);
        n++;
      }
    }
    if (vx_connectors == 0) {
        printf("no connectors!");
        return;
    }
    GO_WINDOW(3);
    vx_connector = (inl(BASE + VX_W3_INTERNAL_CFG) 
                        & INTERNAL_CONNECTOR_MASK) 
                        >> INTERNAL_CONNECTOR_BITS;
    if (vx_connector & 0x10) {
        vx_connector &= 0x0f;
        printf("[*%s*]", conn_tab[vx_connector].name);
        printf(": disable 'auto select' with DOS util!");
    } else {
        printf("[*%s*]", conn_tab[vx_connector].name);
    }
}

static void            
vxsetlink(void)
{       
    int i, j, k;
    char *reason, *warning;
    static short prev_flags;
    static char prev_conn = -1;

    if (prev_conn == -1) {
        prev_conn = vx_connector;
    }

    i = vx_connector;       /* default in EEPROM */
    reason = "default";
    warning = 0;

    if ((vx_connectors & conn_tab[vx_connector].bit) == 0) {
        warning = "strange connector type in EEPROM.";
        reason = "forced";
        i = CONNECTOR_UTP;
    }

        if (warning != 0) {
            printf("warning: %s\n", warning);
        }
        printf("selected %s. (%s)\n", conn_tab[i].name, reason);

    /* Set the selected connector. */
    GO_WINDOW(3);
    j = inl(BASE + VX_W3_INTERNAL_CFG) & ~INTERNAL_CONNECTOR_MASK;
    outl(j | (i <<INTERNAL_CONNECTOR_BITS), BASE + VX_W3_INTERNAL_CFG);

    /* First, disable all. */
    outw(STOP_TRANSCEIVER, BASE + VX_COMMAND);
    DELAY(8000);
    GO_WINDOW(4);
    outw(0, BASE + VX_W4_MEDIA_TYPE);

    /* Second, enable the selected one. */
    switch(i) {
      case CONNECTOR_UTP:
        GO_WINDOW(4);
        outw(ENABLE_UTP, BASE + VX_W4_MEDIA_TYPE);
        break;
      case CONNECTOR_BNC:
        outw(START_TRANSCEIVER,BASE + VX_COMMAND);
        DELAY(8000);
        break;
      case CONNECTOR_TX:
      case CONNECTOR_FX:
        GO_WINDOW(4);
        outw(LINKBEAT_ENABLE, BASE + VX_W4_MEDIA_TYPE);
        break;
      default:  /* AUI and MII fall here */
        break;
    }
    GO_WINDOW(1); 
}

static void t595_disable(struct nic *nic)
{
    outw(STOP_TRANSCEIVER, BASE + VX_COMMAND);
    DELAY(8000);
    GO_WINDOW(4);
    outw(0, BASE + VX_W4_MEDIA_TYPE);
    GO_WINDOW(1);
}

/**************************************************************************
ETH_PROBE - Look for an adapter
***************************************************************************/
struct nic *t595_probe(struct nic *nic, unsigned short *probeaddrs, struct pci_device *pci)
{
	int i;
	unsigned short *p;

	if (probeaddrs == 0 || probeaddrs[0] == 0)
		return 0;
/*	eth_nic_base = probeaddrs[0] & ~3; */
	eth_nic_base = pci->ioaddr;

	GO_WINDOW(0);
	outw(GLOBAL_RESET, BASE + VX_COMMAND);
	VX_BUSY_WAIT;

	vxgetlink();

/*
	printf("\nEEPROM:");
	for (i = 0; i < (EEPROMSIZE/2); i++) {
	  printf("%x:", get_e(i));
	}
	printf("\n");
*/
	/*
	* Read the station address from the eeprom
	*/
	p = (unsigned short *) nic->node_addr;
	for (i = 0; i < 3; i++) {
		GO_WINDOW(0);
		p[i] = htons(get_e(EEPROM_OEM_ADDR_0 + i));
		GO_WINDOW(2);
		outw(ntohs(p[i]), BASE + VX_W2_ADDR_0 + (i * 2));
	}

	printf("Ethernet address: ");
	for(i=0; i<5; i++) {
		printf("%b:",nic->node_addr[i]);
	}
	printf("%b\n",nic->node_addr[i]);

	t595_reset(nic);
	nic->reset = t595_reset;
	nic->poll = t595_poll;
	nic->transmit = t595_transmit;
	nic->disable = t595_disable;
	return nic;

}

/*
 * Local variables:
 *  c-basic-offset: 8
 * End:
 */

