#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#include <sys/sysmacros.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/major.h>
#include <errno.h>
#include <assert.h>
#include <sys/poll.h>
#include <unistd.h>

#include <sys/socket.h>
#include <linux/if.h>
#include <linux/if_tun.h>

#include <pthread.h>

// build process does not support including the following header
// #include <linux/if_arp.h>
// so we copy the definition we need
#define ARPHRD_ETHER    1               /* Ethernet 10Mbps              */

#include <net/ethernet.h>

// Etherboot headers

#define STDINT_H
typedef unsigned           size_t;

typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned long      uint32_t;
typedef unsigned long long uint64_t;

typedef signed char        int8_t;
typedef signed short       int16_t;
//typedef signed long        int32_t;
typedef signed long long   int64_t;

typedef signed char	   s8;
typedef unsigned char      u8;

typedef signed short       s16;
typedef unsigned short     u16;

typedef signed int         s32;
typedef unsigned int       u32;

typedef signed long long   s64;
typedef unsigned long long u64;

#define TFTP_MAX_PACKET         1432 /* 512 */

#define PAGE_SIZE (4096)

#define DBG_LEVEL (1)
#define DBG(...) do { if(DBG_LEVEL) printf(__VA_ARGS__); } while(0)

#include <if_ether.h>
#include <pxe.h>
#include <callbacks_arch.h>

#undef inb
#undef inb_p
#undef inw
#undef inw_p
#undef inl
#undef inl_p


#undef outb
#undef outb_p
#undef outw
#undef outw_p
#undef outl
#undef outl_p

#include <sys/io.h>
#include <asm/ldt.h>
#include <linux/unistd.h>
#include "undi.h"

#define PKT_BUF_SIZE (1600)

#define IFCONFIG_PATH "/sbin/ifconfig"


int modify_ldt(int func, void *ptr, unsigned long bytecount);
static void hexdump(char *loc, int len);
static pxe_t *hunt_pixie ( void ) ;
static int tap_alloc(char *dev, char *hw_addr);

void *entrypoint32;

pxe_t *g_pxe;


#define TX_RING_LEN (1024)
int tx_ring_head = 0;
int tx_ring_tail = 0;
int tx_pending = 0;

int use_multithreaded_driver = 0;
int tap_poll_interval = 1;
int undi_poll_interval = 1;

struct RingEntry {
	char data[PKT_BUF_SIZE];
	int len;
};

struct RingEntry tx_ring[TX_RING_LEN];

unsigned long int virt_offset = 0;

#define DEV_FNAME "/dev/undimem"

#define TUN_CTL_FNAME "/dev/net/tun"

// Compatibility definitions with Etherboot UNDI driver code

#define UNDI_STATUS(PXS) ( (PXS)->Status == PXENV_EXIT_SUCCESS ? \
			   "SUCCESS" :				      \
			   ( (PXS)->Status == PXENV_EXIT_FAILURE ?    \
			     "FAILURE" : "UNKNOWN" ) )

#define UNDI_STATUS_BOOL(PXS) ((PXS)->Status == PXENV_EXIT_SUCCESS)

#undef SEGMENT
#undef OFFSET
#define SEGMENT(X)  	((((unsigned int) (X)) >> 4) & 0xf000)
#define OFFSET(X) 	(((unsigned int) (X)) & 0xffff)

#undef VIRTUAL
#define VIRTUAL(SEG,OFF) ( (char*) (((unsigned)(SEG)) << 4) + (unsigned)(OFF) )

typedef struct undi_base_mem_xmit_data {
        MAC_ADDR                destaddr;
        t_PXENV_UNDI_TBD        tbd;
} undi_base_mem_xmit_data_t;

struct {
	t_PXENV_ANY *pxs;
	undi_base_mem_xmit_data_t *xmit_data;
	char *tx_buffer;
	char *rx_buffer;

	char hwaddr[ETH_ALEN];
} undi;

int tap_fd;

int call_pxe(int opcode) {
	short retval;
	int ignored1, ignored2, ignored3, ignored4, ignored5;
	
	// XXX Address is passed in as if in real mode
	assert(!(((unsigned int) undi.pxs) & ~0xfffff));
	short seg = SEGMENT( undi.pxs );
	short offset = OFFSET( undi.pxs );
	asm (
	     "pushw %%bx\n" // seg
	     "pushw %%dx\n" // offset
	     "pushw %%ax\n" // opcode
	     "call *%%ecx\n"
	     : "=a" (retval), "=b" (ignored1), "=c" (ignored2), "=d" (ignored3),
	       "=D" (ignored4), "=S" (ignored5)
	     :  "a" (opcode), "b" (seg), "d" (offset), "c" (entrypoint32) );
	return retval;
}

int *log_position;
char *log_base;
char caller_log_buf[1024];

char *arg_buffer;
int arg_buffer_len;

int undi_opened = 0;

void print_log(void) {
	printf(caller_log_buf);
}

static int undi_get_information(void) {
	/* based on eb_pxenv_undi_get_information */
	int success = 0;
	memset ( undi.pxs, 0, sizeof ( *undi.pxs ) );
	DBG ( "PXENV_UNDI_GET_INFORMATION => (void)\n" );
	success = call_pxe ( PXENV_UNDI_GET_INFORMATION);
	DBG ( "PXENV_UNDI_GET_INFORMATION <= Status=%s "
	      "BaseIO=%hx IntNumber=%hx ...\n"
	      "... MaxTranUnit=%hx HwType=%hx HwAddrlen=%hx ...\n"
	      "... CurrentNodeAddress=%p PermNodeAddress=%p ...\n"
	      "... ROMAddress=%hx RxBufCt=%hx TxBufCt=%hx\n",
	      UNDI_STATUS(undi.pxs),
	      undi.pxs->undi_get_information.BaseIo,
	      undi.pxs->undi_get_information.IntNumber,
	      undi.pxs->undi_get_information.MaxTranUnit,
	      undi.pxs->undi_get_information.HwType,
	      undi.pxs->undi_get_information.HwAddrLen,
	      undi.pxs->undi_get_information.CurrentNodeAddress,
	      undi.pxs->undi_get_information.PermNodeAddress,
	      undi.pxs->undi_get_information.ROMAddress,
	      undi.pxs->undi_get_information.RxBufCt,
	      undi.pxs->undi_get_information.TxBufCt );

	DBG("Success = %d\n", success);
	return !success;
}

static int undi_set_station_address(void) {
	/* based on eb_pxenv_undi_set_station_address */

	/* This will spuriously fail on some cards.  Ignore failures.
	 * We only ever use it to set the MAC address to the card's
	 * permanent value anyway, so it's a useless call (although we
	 * make it because PXE spec says we should).
	 */
	DBG ( "PXENV_UNDI_SET_STATION_ADDRESS => "
	      "StationAddress=%p\n",
	      undi.pxs->undi_set_station_address.StationAddress );
	call_pxe ( PXENV_UNDI_SET_STATION_ADDRESS );
	DBG ( "PXENV_UNDI_SET_STATION_ADDRESS <= Status=%s\n",
	      UNDI_STATUS(undi.pxs) );
	return 1;
}

static int undi_open(void) {
	/* based on eb_pxenv_undi_open */

	int success = 0;

	undi.pxs->undi_open.OpenFlag = 0;
	undi.pxs->undi_open.PktFilter = FLTR_DIRECTED | FLTR_BRDCST;
	
	/* Multicast support not yet implemented */
	undi.pxs->undi_open.R_Mcast_Buf.MCastAddrCount = 0;
	DBG ( "PXENV_UNDI_OPEN => OpenFlag=%hx PktFilter=%hx "
	      "MCastAddrCount=%hx\n",
	      undi.pxs->undi_open.OpenFlag, undi.pxs->undi_open.PktFilter,
	      undi.pxs->undi_open.R_Mcast_Buf.MCastAddrCount );
	call_pxe ( PXENV_UNDI_OPEN );
	DBG ( "PXENV_UNDI_OPEN <= Status=%s\n", UNDI_STATUS(undi.pxs) );

	success = UNDI_STATUS_BOOL(undi.pxs);
	if ( success ) undi_opened = 1;
	return success;
}

#undef DBG_LEVEL
#define DBG_LEVEL (0)
static int undi_transmit_packet(void) {
	/* based on eb_pxenv_undi_transmit_packet */

	static const uint8_t broadcast[] = { 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF };

	/* XMitFlag selects unicast / broadcast */
	if ( memcmp ( undi.xmit_data->destaddr, broadcast,
		      sizeof(broadcast) ) == 0 ) {
		undi.pxs->undi_transmit.XmitFlag = XMT_BROADCAST;
	} else {
		undi.pxs->undi_transmit.XmitFlag = XMT_DESTADDR;
	}

	/* Zero reserved dwords */
	undi.pxs->undi_transmit.Reserved[0] = 0;
	undi.pxs->undi_transmit.Reserved[1] = 0;

	/* Segment:offset pointer to DestAddr in base memory */
	undi.pxs->undi_transmit.DestAddr.segment =
		SEGMENT( undi.xmit_data->destaddr );
	undi.pxs->undi_transmit.DestAddr.offset =
		OFFSET( undi.xmit_data->destaddr );

	/* Segment:offset pointer to TBD in base memory */
	undi.pxs->undi_transmit.TBD.segment = SEGMENT( &undi.xmit_data->tbd );
	undi.pxs->undi_transmit.TBD.offset = OFFSET( &undi.xmit_data->tbd );

	/* Use only the "immediate" part of the TBD */
	undi.xmit_data->tbd.DataBlkCount = 0;
	
	DBG ( "PXENV_UNDI_TRANSMIT_PACKET => Protocol=%hx XmitFlag=%hx ...\n"
	      "... DestAddr=%hx:%hx TBD=%hx:%hx ...\n",
	      undi.pxs->undi_transmit.Protocol,
	      undi.pxs->undi_transmit.XmitFlag,
	      undi.pxs->undi_transmit.DestAddr.segment,
	      undi.pxs->undi_transmit.DestAddr.offset,
	      undi.pxs->undi_transmit.TBD.segment,
	      undi.pxs->undi_transmit.TBD.offset );
	DBG ( "... TBD { ImmedLength=%hx Xmit=%hx:%hx DataBlkCount=%hx }\n",
	      undi.xmit_data->tbd.ImmedLength,
	      undi.xmit_data->tbd.Xmit.segment,
	      undi.xmit_data->tbd.Xmit.offset,
	      undi.xmit_data->tbd.DataBlkCount );
	call_pxe ( PXENV_UNDI_TRANSMIT );
	DBG ( "PXENV_UNDI_TRANSMIT_PACKET <= Status=%s (0x%x)\n",
	      UNDI_STATUS(undi.pxs), undi.pxs->Status );

	int success = UNDI_STATUS_BOOL(undi.pxs);
	if(!success) {
		printf("Possibly out of UNDI tx rings\n");
#if 0
		// for now, don't clear the pending flag, just keep blindly trying
		tx_pending = 1;
#endif
	}
	return success;
}
#undef DBG_LEVEL
#define DBG_LEVEL (1)

static int transmit_packet(char *buf, int len) {
	if(len < sizeof(struct ether_header)) {
		printf("Length is too short\n");
		return 0;
	}

	/* Inspect the ethernet type */
	struct ether_header *eth = (struct ether_header *)buf;
	int skip_header = 1;
	switch(ntohs(eth->ether_type)) {
	case ETHERTYPE_IP:
		undi.pxs->undi_transmit.Protocol = P_IP;
		break;
	case ETHERTYPE_ARP:
		undi.pxs->undi_transmit.Protocol = P_ARP;
		break;
	case ETHERTYPE_REVARP:
		undi.pxs->undi_transmit.Protocol = P_RARP;
		break;
	default:
		printf("Unknown packet type %x!\n", eth->ether_type);
		skip_header = 0;
		undi.pxs->undi_transmit.Protocol = P_UNKNOWN;
		return 0;
	}
	memcpy(undi.xmit_data->destaddr, eth->ether_dhost, ETH_ALEN);

	/* Skip the Ethernet header */
	char *stripped_pkt;
	if(skip_header) {
		stripped_pkt = buf + sizeof(struct ether_header);
		len -= sizeof(struct ether_header);
	} else {
		stripped_pkt = buf;
	}

	/* Move packet to basemem buffer */
	memcpy(undi.tx_buffer, stripped_pkt, len);

	undi.xmit_data->tbd.ImmedLength = len;
	undi.xmit_data->tbd.Xmit.segment = SEGMENT(undi.tx_buffer);
	undi.xmit_data->tbd.Xmit.offset = OFFSET(undi.tx_buffer);
	undi.xmit_data->tbd.DataBlkCount = 0;
	return undi_transmit_packet();
}

static void transmit_next(void) {
	if(!tx_pending && tx_ring_head != tx_ring_tail) {
		// can send, and packets pending
		transmit_packet(tx_ring[tx_ring_head].data, tx_ring[tx_ring_head].len);

		tx_ring[tx_ring_head].len = 0;
		tx_ring_head = (tx_ring_head + 1) % TX_RING_LEN;
	}
}

static int undi_isr(void) {
	/* Based on eb_pxenv_undi_isr */
	int success = -1;

#if 0
	DBG ( "PXENV_UNDI_ISR => FuncFlag=%hx\n",
	      undi.pxs->undi_isr.FuncFlag );	
#endif
	success = call_pxe ( PXENV_UNDI_ISR );
#if 0
	DBG ( "PXENV_UNDI_ISR <= Status=%s FuncFlag=%hx BufferLength=%hx ...\n"
	      "... FrameLength=%hx FrameHeaderLength=%hx Frame=%hx:%hx "
	      "ProtType=%hhx ...\n... PktType=%hhx\n",
	      UNDI_STATUS(undi.pxs), undi.pxs->undi_isr.FuncFlag,
	      undi.pxs->undi_isr.BufferLength,
	      undi.pxs->undi_isr.FrameLength,
	      undi.pxs->undi_isr.FrameHeaderLength,
	      undi.pxs->undi_isr.Frame.segment,
	      undi.pxs->undi_isr.Frame.offset,
	      undi.pxs->undi_isr.ProtType,
	      undi.pxs->undi_isr.PktType );
#endif
	return !success;
}

static int do_undi_poll(void) {
	// Based on Etherboot undi_poll()

	undi.pxs->undi_isr.FuncFlag = PXENV_UNDI_ISR_IN_START;
	if ( ! undi_isr() ) {
		printf("UNDI isr error (0)\n");
		return 0;
	}
	if ( undi.pxs->undi_isr.FuncFlag == PXENV_UNDI_ISR_OUT_NOT_OURS ) {
		// no interrupt detected
		return 0;
	}
	undi.pxs->undi_isr.FuncFlag = PXENV_UNDI_ISR_IN_PROCESS;
	if ( ! undi_isr() ) {
		printf("UNDI isr error (1)\n");
		return 0;
	}
	while ( undi.pxs->undi_isr.FuncFlag != PXENV_UNDI_ISR_OUT_DONE ) {
		switch ( undi.pxs->undi_isr.FuncFlag ) {
		case PXENV_UNDI_ISR_OUT_TRANSMIT:
			printf("Got Tx interrupt\n");
			tx_pending = 0;
			transmit_next();
			/* We really don't care about transmission complete
			 * interrupts.
			 */
			break;
		case PXENV_UNDI_ISR_OUT_BUSY:
			/* This should never happen.
			 */
			printf ( "UNDI ISR thinks it's being re-entered!\n"
				 "Aborting receive\n" );
			return 0;
		case PXENV_UNDI_ISR_OUT_RECEIVE: {
			/* Copy data to receive buffer */
#if 0
			printf("Rx: %x:%x %d / %d\n", undi.pxs->undi_isr.Frame.segment,
			       undi.pxs->undi_isr.Frame.offset,
			       undi.pxs->undi_isr.BufferLength, undi.pxs->undi_isr.FrameLength );
#endif
			char recv_buf[sizeof(struct tun_pi) + PKT_BUF_SIZE];
			struct tun_pi *tp = (struct tun_pi *)recv_buf;
			char *data = recv_buf + 4;
			int len = 4 + undi.pxs->undi_isr.BufferLength;

			tp->flags = 0;
			tp->proto = htons(ETH_P_IP);

			char *pkt_src = VIRTUAL(undi.pxs->undi_isr.Frame.segment,
						undi.pxs->undi_isr.Frame.offset);
#if 0
			printf("Attempt stuffing of tap packet from %p, len = %d\n", 
			       pkt_src, undi.pxs->undi_isr.BufferLength);
#endif
			memcpy(data, pkt_src, undi.pxs->undi_isr.BufferLength);
			write(tap_fd, recv_buf, len);
			break;
		}
		default:
			printf ( "UNDI ISR returned bizzare status code %d\n",
				 undi.pxs->undi_isr.FuncFlag );
		}
		undi.pxs->undi_isr.FuncFlag = PXENV_UNDI_ISR_IN_GET_NEXT;
		if ( ! undi_isr() ) {
			printf("UNDI isr error (2)\n");
			return 0;
		}
	}
	return 1;
}

static int undi_startup(void) {
	if(!undi_get_information()) {
		printf("pxenv_get_information error\n");
		return 0;
	}
	memcpy(undi.hwaddr, undi.pxs->undi_get_information.PermNodeAddress, 
	       sizeof(undi.pxs->undi_get_information.PermNodeAddress));
	memmove ( &undi.pxs->undi_set_station_address.StationAddress,
		  &undi.pxs->undi_get_information.PermNodeAddress,
		  sizeof (undi.pxs->undi_set_station_address.StationAddress) );
	if(!undi_set_station_address()) {
		printf("error setting station address\n");
		return 0;
	}
	if(!undi_open()) {
		printf("error opening undi\n");
		return 0;
	}
	assert(undi_opened);
	return 1;
}

pthread_mutex_t dev_mutex = PTHREAD_MUTEX_INITIALIZER;

struct tap_ctl {
	int fd;
};

void do_tap_poll(int tap_fd) {
	struct pollfd poll_ctl[1] = {
		{
			.fd = tap_fd,
			.events = POLLIN,
			.revents = 0
		}
	};
	poll(poll_ctl, 1, tap_poll_interval);

	if(poll_ctl[0].revents & POLLIN) {
		// Get packet, and queue for send
		if((tx_ring_tail + 1) % TX_RING_LEN == tx_ring_head) {
			printf("About to overflow tx ring\n");
			return;
		}
		char send_buf[sizeof(struct tun_pi) + PKT_BUF_SIZE];
		struct tun_pi *tp = (struct tun_pi *)send_buf;
		char *pkt_data = send_buf + 4;

		int len = read(tap_fd, send_buf, sizeof(send_buf));
		if(tp->flags & TUN_PKT_STRIP) {
			printf("Truncated packet!\n");
			return;
		}
		len -= 4;
		assert(len >= 0);

		pthread_mutex_lock(&dev_mutex);
		memcpy(tx_ring[tx_ring_tail].data, pkt_data, len);
		tx_ring[tx_ring_tail].len = len;
		tx_ring_tail = (tx_ring_tail + 1) % TX_RING_LEN;

		transmit_next();
		pthread_mutex_unlock(&dev_mutex);
	}
}

void *tap_loop(void *_tap_ctl) {
	struct tap_ctl *tap_ctl = (struct tap_ctl*)_tap_ctl;
	int tap_fd = tap_ctl->fd;

	while(1) {
		do_tap_poll(tap_fd);
	}
}

int main(int argc, char **argv) {
	struct stat file_stat;
	if(argc >= 2) {
		use_multithreaded_driver = 1;
	}
	printf("Using multithreaded driver: %s\n", use_multithreaded_driver ? "yes" : "no");

	if(stat(DEV_FNAME, &file_stat) != 0) {
		if(errno == ENOENT) {
			mode_t  mode = S_IFCHR;
			if(mknod(DEV_FNAME, mode, 
				 makedev(MISC_MAJOR, UNDI_MEM_MINOR)) != 0) {
				printf("mknod(%s): error %d!\n", DEV_FNAME, errno);
				exit(-1);
			}
		} else {
			printf("stat(%s): Unknown error!\n", DEV_FNAME);
			exit(-1);
		}
	}

	if(chmod(DEV_FNAME, S_IRUSR | S_IWUSR) != 0) {
		printf("chmod(%s): error\n", DEV_FNAME);
		exit(-1);
	}
	int undi_mem_fd = open(DEV_FNAME, O_RDWR);
	if(undi_mem_fd < 0) {
		printf("Could not open %s\n", DEV_FNAME);
		exit(-1);
	}

	printf("About to mmap()\n");
	if(ioctl(undi_mem_fd, UNDI_MEM_MAP_PXE_SEARCH) != 0) {
		printf("ioctl failed\n");
		exit(-1);
	}
	void *base = mmap(PXE_SEARCH_MAP_BASE, PXE_SEARCH_MAP_LENGTH,
			  PROT_READ | PROT_WRITE | PROT_EXEC,
			  MAP_FIXED | MAP_PRIVATE,
			  undi_mem_fd, 0);
	// printf("mmap fd = %d rv = %d errno = %d\n", undi_mem_fd, rv, errno);
	if(base == MAP_FAILED) {
		printf("mmap() error %d!\n", errno);
		return -1;
	}
	printf("mmap() => %p\n", base);

#if 0
	printf("Dumping\n");
	int dumpfile = open(DUMP_FNAME, O_WRONLY);
	if(dumpfile < 0) {
		printf("error opening dumpfile\n");
	} else {
		write(dumpfile, );
	}
#endif

	printf("About to hunt\n");
	g_pxe = hunt_pixie();

	if(g_pxe == NULL) {
		printf("Could not find pxe!\n");
		return -1;
	}

	/* Etherboot-specific hacks to get log and base memory space */

	log_position = (int*)((char*)(g_pxe) + sizeof(*g_pxe));
	*(char **)(log_position + 1) = caller_log_buf;
	log_base = (char *) (log_position + 2);
	printf(" initial data = %s\n", log_base + 1);

	// skip past prot_log_tail, caller_log_buf, and prot_log_data
	arg_buffer = (char*) log_position + 8 + 1024;

	arg_buffer_len = 4096;
	undi.pxs = (t_PXENV_ANY *) arg_buffer;
	undi.xmit_data = (void*)(undi.pxs + 1);
	undi.tx_buffer = (void *)(undi.xmit_data + 1);
	undi.rx_buffer = undi.tx_buffer + PKT_BUF_SIZE;

	entrypoint32 = (void*) ((g_pxe->EntryPointESP.segment << 16) | 
				g_pxe->EntryPointESP.offset);

	uint32_t pxe_base = g_pxe->BC_Data.Phy_Addr;
	uint32_t pxe_length = (g_pxe->BC_Data.Seg_Addr << 16) | 
		g_pxe->BC_Data.Seg_Size;

	printf("PXE segment is %p:%d, entry point is %p\n", (void *) pxe_base, (int)pxe_length, entrypoint32);

	printf("Using undimem device to map in pxe segment pages\n");
	void *pxe_base_page_aligned = (void *)(pxe_base & ~(PAGE_SIZE - 1));
	uint32_t pxe_length_page_aligned = (((pxe_base + pxe_length) - (uint32_t)pxe_base_page_aligned) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
	struct UNDI_Exec_Ctl exec_ctl = {
		.mem_base = pxe_base_page_aligned,
		.mem_length = pxe_length_page_aligned,
	};
	if(ioctl(undi_mem_fd, UNDI_MEM_MAP_UNDI_EXEC, &exec_ctl) != 0) {
		printf("ioctl error %d\n", errno);
		exit(-1);
	}
	void *exec_base = mmap(pxe_base_page_aligned, pxe_length_page_aligned,
			  PROT_READ | PROT_WRITE | PROT_EXEC,
			  MAP_FIXED | MAP_PRIVATE,
			  undi_mem_fd, 0);
	if(exec_base == MAP_FAILED) {
		printf("mmap execution region failed\n");
		exit(-1);
	}
	printf("exec_base = %p\n", exec_base);

	int num_descs = g_pxe->SegDescCn;
	printf("Modifying LDT, need %d entries\n", num_descs);

	// sanity check alignment

	SEGDESC_t *descs = &g_pxe->Stack;
	if(descs + 1 != &g_pxe->UNDIData) {
		printf("UNDI segment alignment is weird!\n");
		return -1;
	}

	SEGSEL_t first_selector;
	printf("Updating PXE with first selector %x\n", first_selector);
	/*
	  API Hack:
		[3] = BC_Data -- CS
		[4] = BC_Code -- SS,DS,ES
	*/

	// Index = 3, LDT, CPL3
	first_selector = (0 << 3) | (0x4 /* LDT */ | 0x3 /* RPL 3 */);
	g_pxe->FirstSelector = first_selector;

	// modify ldt
	struct modify_ldt_ldt_s mod_entry_cs = {
		.entry_number = 3,
		.base_addr = pxe_base,
		.limit = pxe_length_page_aligned / PAGE_SIZE,
		.seg_32bit = 1,
		.contents = 0x2, // Code, non-conforming
		.read_exec_only = 0, // Execute/Read
		.limit_in_pages = 1,
		.seg_not_present = 0,
		.useable = 1,
	};
	struct modify_ldt_ldt_s mod_entry_ds = mod_entry_cs;
	mod_entry_ds.entry_number = 4;
	mod_entry_ds.contents = 0; // Normal data
	mod_entry_ds.read_exec_only = 0; // read/write

	// use "new mode" modify_ldt() interface (based on 2.6.16 kernel's sys_modify_ldt)
	if(modify_ldt(0x11, &mod_entry_cs, sizeof(mod_entry_cs)) == -1) {
		printf("Modify ldt(cs) error %d\n", errno); // EINVAL
	}
	if(modify_ldt(1, &mod_entry_ds, sizeof(mod_entry_ds)) == -1) {
		printf("Modify ldt(ds) error %d\n", errno);
	}

	// Setting IOPL3
	printf("Setting IOPL3\n");
	if(iopl(3) != 0) {
		printf("IOPL error %d\n", errno);
		return -1;
	}

	if(!undi_startup()) {
		printf("UNDI startup failed\n");
		print_log();
		exit(-1);
	}

	// Create tap device
	char tapdev_name[IFNAMSIZ] = "";
	tap_fd = tap_alloc(tapdev_name, undi.hwaddr);
	if(tap_fd < 0) {
		printf("Could not allocate tap device\n");
		exit(-1);
	}
	printf("Using tap device %s\n", tapdev_name);

	if(use_multithreaded_driver) {
		tap_poll_interval = -1;
		undi_poll_interval = 10;

		pthread_t tap_thread;
		struct tap_ctl tap_ctl = {
			.fd = tap_fd
		};

		printf("Forking tap thread\n");
		int rv = pthread_create(&tap_thread, NULL, tap_loop, &tap_ctl);
		if(rv != 0) {
			printf("Could not fork tap thread\n");
			exit(-1);
		}
		printf("Entering device loop\n");

		while(1) {
			pthread_mutex_lock(&dev_mutex);

			struct timespec ts = {
				.tv_sec = 0,
				.tv_nsec = undi_poll_interval * 1000000
			};
			nanosleep(&ts, NULL);

			do_undi_poll();
			pthread_mutex_unlock(&dev_mutex);
		}
	} else {
		tap_poll_interval = 1;
		undi_poll_interval = 0;

		printf("Entering device loop\n");
		while(1) {
			do_tap_poll(tap_fd);
			do_undi_poll();
		}
	}
	print_log();
	return 0;
}

// Utility and PXE scan functions based on Etherboot code

/**************************************************************************
 * Utility functions
 **************************************************************************/

/* Checksum a block.
 */

static uint8_t checksum ( void *block, size_t size ) {
	uint8_t sum = 0;
	uint16_t i = 0;
	for ( i = 0; i < size; i++ ) {
		sum += ( ( uint8_t * ) block )[i];
	}
	return sum;
}

/* Print the status of a !PXE structure
 */

static void pxe_dump ( pxe_t *pxe ) {
	printf ( "API %hx:%hx ( %hx:%hx ) St %hx:%hx UD %hx:%hx UC %hx:%hx "
		 "BD %hx:%hx BC %hx:%hx\n",
		 pxe->EntryPointSP.segment, pxe->EntryPointSP.offset,
		 pxe->EntryPointESP.segment, pxe->EntryPointESP.offset,
		 pxe->Stack.Seg_Addr, pxe->Stack.Seg_Size,
		 pxe->UNDIData.Seg_Addr, pxe->UNDIData.Seg_Size,
		 pxe->UNDICode.Seg_Addr, pxe->UNDICode.Seg_Size,
		 pxe->BC_Data.Seg_Addr, pxe->BC_Data.Seg_Size,
		 pxe->BC_Code.Seg_Addr, pxe->BC_Code.Seg_Size );
}


static pxe_t *hunt_pixie ( void ) {
	static uint32_t ptr = 0;
	pxe_t *pxe = NULL;

	printf ( "Hunting for pixies..." );
	if ( ptr == 0 ) ptr = 0xa0000;
	while ( ptr > 0x10000 ) {
		ptr -= 16;
		pxe = (pxe_t *) phys_to_virt ( ptr );
		if ( memcmp ( pxe->Signature, "!PXE", 4 ) == 0 ) {
			printf ( "found !PXE at %08x...", (int)ptr );
			if ( checksum ( pxe, sizeof(pxe_t) ) != 0 ) {
				printf ( "invalid checksum\n..." );
				continue;
			}
			printf ( "ok, really found at %p\n", (void *)ptr );
			pxe_dump(pxe);
			return pxe;
		}
	}
	printf ( "none found\n" );
	ptr = 0;
	return NULL;
}

static void hexdump(char *loc, int len) {
	int i;
	for(i=0; i < len; i++) {
		printf("%02x ", (int)(unsigned char)loc[i]);
		if(i > 0 && (i + 1) %16 == 0) {
			printf("\n");
		}
	}
	if(i % 16 != 0) {
		printf("\n");
	}
}

void peek(char *location, int len) {
	hexdump(location, len);
}

static void ifconfig(char *dev, char *command) {
	char cmd_str[80];
	sprintf(cmd_str, "%s %s %s", IFCONFIG_PATH, dev, command);
	printf("Issuing '%s'\n", cmd_str);
	system(cmd_str);
}

static int tap_alloc(char *dev, char *hwaddr)
{
	struct ifreq ifr;
	int fd, err;

	if( (fd = open(TUN_CTL_FNAME, O_RDWR)) < 0 ) {
		printf("error opening tun ctl\n");
		return -1;
	}

	memset(&ifr, 0, sizeof(ifr));

	/* Flags: IFF_TUN   - TUN device (no Ethernet headers)
	 *        IFF_TAP   - TAP device
	 *
	 *        IFF_NO_PI - Do not provide packet information
	 */
	ifr.ifr_flags = IFF_TAP;
	if( *dev )
		strncpy(ifr.ifr_name, dev, IFNAMSIZ);

	if( (err = ioctl(fd, TUNSETIFF, (void *) &ifr)) < 0 ){
		close(fd);
		printf("Tap allocation got error %d\n", err);
		return -1;
	}
	printf("Allocated name is %s\n", ifr.ifr_name);
	strcpy(dev, ifr.ifr_name);

	char etherstr[80] = "hw ether ";
	int i;
	for(i=0; i < ETH_ALEN; i++) {
		sprintf(etherstr + strlen(etherstr), "%02x",
			(int)((unsigned char*)hwaddr)[i]);
	}
	ifconfig(dev, etherstr);

	return fd;
}
