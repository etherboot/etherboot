#ifdef FREEBSD_PXEEMU
#include "etherboot.h"
#include "osdep.h"
#include "nic.h"

#define UDP_MAX_PAYLOAD	(ETH_FRAME_LEN - ETH_HLEN - sizeof(struct iphdr) \
			 - sizeof(struct udphdr))
struct udppacket_t {
	struct iphdr	ip;
	struct udphdr	udp;
	uint8_t 	payload[UDP_MAX_PAYLOAD];
};

static int pxeemu_entry(struct v86 *v86_p, int *v86_call_flag,
			int pxeemu_func_nr, vm_offset_t pxeemu_func_arg);

static pxenv_t pxenv = {
        {'P','X','E','N','V','+'},      /* Signature    */
        0x0100,                         /* Version      */
        sizeof(pxenv_t),                /* Length       */
        0,                              /* Checksum     */
        {0, 0},                         /* RMEntry      */
        (uint32_t)pxeemu_entry,		/* PMOffset     */
        0,                              /* PMSelector   */
        0,                              /* StackSeg     */
        0,                              /* StackSize    */
        0,                              /* BC_CodeSeg   */
        0,                              /* BC_CodeSize  */
        0,                              /* BC_DataSeg   */
        0,                              /* BC_DataSize  */
        0,                              /* UNDIDataSeg  */
        0,                              /* UNDIDataSize */
        0,                              /* UNDICodeSeg  */
        0,                              /* UNDICodeSize */
        {0, 0}                          /* !PXEPtr      */
};

static jmpbuf		pxeemu_v86call_jbuf;
static jmpbuf		pxeemu_entry_jbuf;
extern char		pxeemu_nbp_active;

#define PXEEMU_EXIT_ADJ		2
#define PXENV_EXIT_SUCCESS	(0x0000 + PXEEMU_EXIT_ADJ)
#define PXENV_EXIT_FAILURE	(0x0001 + PXEEMU_EXIT_ADJ)
#define PXEEMU_EXIT_V86INT	(0x0002 + PXEEMU_EXIT_ADJ)

#define PXENV_STATUS_SUCCESS	(PXENV_EXIT_SUCCESS - PXEEMU_EXIT_ADJ)
#define PXENV_STATUS_FAILURE	(PXENV_EXIT_FAILURE - PXEEMU_EXIT_ADJ)

#define	V86INT()	do {	*pxeemu_v86_flag = 1;			\
				if(setjmp(pxeemu_v86call_jbuf) == 0)	\
					longjmp(pxeemu_entry_jbuf,	\
						PXEEMU_EXIT_V86INT);	\
				*pxeemu_v86_flag = 0;			\
			} while(0);

static struct v86	*v86_p;
static int		*pxeemu_v86_flag;
static int		pxeemu_func_nr;
static vm_offset_t	pxeemu_func_arg;
static int		retval;

static __inline unsigned int min(unsigned int a, unsigned int b) { return (a < b ? a : b); }

static unsigned long pxeemu_nbp_addr = 0x7C00;

static int pxe_download(unsigned char *data, unsigned int len, int eof);
os_download_t pxe_probe(unsigned char *data, unsigned int len)
{
	if (*((uint32_t *)(data +2)) == 0x42455850L) {
		printf("(PXE)");
		return pxe_download;
	}
	return 0;
}

static int pxe_download(unsigned char *data, unsigned int len, int eof)
{
	memcpy(phys_to_virt(pxeemu_nbp_addr), data, len);
	pxeemu_nbp_addr += len;
	if (eof) {
		uint8_t val, *ptr, counter;
		
		ptr = (uint8_t*) &pxenv;
		val = 0;
		for(counter = 0; counter < pxenv.Length; ++counter, 
			    ++ptr)
			val += *ptr;
		pxenv.Checksum = 0xff - val + 1;
		printf("\n");
		
		
		__asm__ __volatile__	(
			"movb $1, pxeemu_nbp_active\n"
			"call _prot_to_real\n"
			".code16\n"
			"movw %0, %%ax\n"                    
			"movw %%ax, %%es\n"
			"movl %1, %%ebx\n"
			"ljmp $0x0,$0x7c00\n"
			".code32\n"
			: : "i" (RELOC >> 4), 
			"g" (((vm_offset_t)&pxenv) - RELOC));
	}
	return 0;
}

static int await_udp_pxe(int ival, void *ptr,
	unsigned short ptype, struct iphdr *ip, struct udphdr *udp)
{
	t_PXEENV_UDP_READ *s = ptr;
	if (!udp) 
		return 0;
	if ((s->dest_ip != 0) && (s->dest_ip != ip->dest.s_addr))
		return 0;
	if ((s->d_port != 0) && (s->d_port != udp->dest))
		return 0;
	return 1;
}

static int pxeemu_entry(struct v86 *x_v86_p, int *x_pxeemu_v86_flag,
			int x_pxeemu_func_nr, vm_offset_t x_pxeemu_func_arg)
{
	v86_p = x_v86_p;
	pxeemu_v86_flag = x_pxeemu_v86_flag;
	pxeemu_func_nr = x_pxeemu_func_nr;
	pxeemu_func_arg = x_pxeemu_func_arg;

	if(*pxeemu_v86_flag == 1)
		longjmp(pxeemu_v86call_jbuf, 1);
	else {
		if( (retval = setjmp(pxeemu_entry_jbuf)) != 0)
			goto pxeemu_exit;

		/* Switch Stacks */
		__asm__("xorl %%eax, %%eax\n"
			"movw initsp, %%ax\n"
			"addl %0, %%eax\n"
			"movl %%eax, %%esp\n"
			: : "i" (RELOC));
	}

	switch(pxeemu_func_nr)
	{
	    case PXENV_GET_CACHED_INFO:
	    {
 		t_PXENV_GET_CACHED_INFO *s =
				(t_PXENV_GET_CACHED_INFO*) pxeemu_func_arg;
		char *buf;
		
		if(s->PacketType != PXENV_PACKET_TYPE_BINL_REPLY
		   || s->Buffer.segment != 0 || s->Buffer.offset != 0) {
			s->Status = PXENV_STATUS_FAILURE;
			retval = PXENV_EXIT_FAILURE;
			break;
		}

		s->Buffer.segment = (RELOC >> 4);
		s->Buffer.offset = (((vm_offset_t)&bootp_data) - RELOC);
		s->BufferSize = sizeof(struct bootpd_t);
		s->BufferLimit = sizeof(struct bootpd_t);
		s->Status = PXENV_STATUS_SUCCESS;
		retval = PXENV_EXIT_SUCCESS;
		break;
	    }
	    case PXENV_UDP_OPEN:
	    {
		t_PXENV_UDP_OPEN *s = (t_PXENV_UDP_OPEN*) pxeemu_func_arg;
		arptable[ARP_CLIENT].ipaddr.s_addr = s->src_ip;
		s->status = PXENV_STATUS_SUCCESS;
		retval = PXENV_EXIT_SUCCESS;		
		break;
	    }
	    case PXENV_UDP_WRITE:
	    {
		t_PXENV_UDP_WRITE *s = (t_PXENV_UDP_WRITE*) pxeemu_func_arg;
		void *ptr = (void*)(s->buffer.segment << 4)+s->buffer.offset;
		struct udppacket_t packet;
		int sz;
		
		sz = min(s->buffer_size, UDP_MAX_PAYLOAD);
		memcpy(packet.payload, ptr, sz);
		sz += sizeof(struct iphdr) + sizeof(struct udphdr);

		if(s->src_port == 0)
			s->src_port = 2069;	/* XXX #define ??? */

		if(udp_transmit(s->ip,
				htons(s->src_port), htons(s->dst_port), 
				sz, &packet) == 0) 
		{
    		    s->status = PXENV_STATUS_FAILURE;
		    retval = PXENV_EXIT_FAILURE;
		} else {
		    s->status = PXENV_STATUS_SUCCESS;
		    retval = PXENV_EXIT_SUCCESS;
		}
		break;
	    }
	    case PXENV_UDP_READ:
	    {
		t_PXENV_UDP_READ *s = (t_PXENV_UDP_READ*) pxeemu_func_arg;
		if(await_reply(await_udp_pxe,
			       arptable[ARP_CLIENT].ipaddr.s_addr, 
			       s, 0) == 0) 
		{
			s->status = PXENV_STATUS_FAILURE;
			retval = PXENV_EXIT_FAILURE;
		} else {
			struct udppacket_t *packet = (struct udppacket_t*)&nic.packet[ETH_HLEN];
			void *ptr = (void*)(s->buffer.segment << 4);
			ptr += s->buffer.offset;

			s->src_ip = packet->ip.src.s_addr;
			s->s_port = packet->udp.src;
			s->buffer_size = min(nic.packetlen 
						- sizeof(struct udppacket_t)
						+ UDP_MAX_PAYLOAD,
					     s->buffer_size);
			memcpy(ptr, packet->payload, s->buffer_size);
			s->status = PXENV_STATUS_SUCCESS;
			retval = PXENV_EXIT_SUCCESS;
		}
		break;
	    }
	    case PXENV_UDP_CLOSE:
	    {
		t_PXENV_UDP_CLOSE *s = (t_PXENV_UDP_CLOSE*) pxeemu_func_arg;
		s->status = PXENV_STATUS_SUCCESS;
		retval = PXENV_EXIT_SUCCESS;		
		break;
	    }
	    case PXENV_UNLOAD_STACK:
	    {
		t_PXENV_UNLOAD_STACK *s = (t_PXENV_UNLOAD_STACK*) pxeemu_func_arg;
		s->Status = PXENV_STATUS_SUCCESS;
		retval = PXENV_EXIT_SUCCESS;		
		break;
	    }
	    case PXENV_UNDI_SHUTDOWN:
	    {
		t_PXENV_UNDI_SHUTDOWN *s = (t_PXENV_UNDI_SHUTDOWN*) pxeemu_func_arg;
		eth_reset();
		s->Status = PXENV_STATUS_SUCCESS;
		retval = PXENV_EXIT_SUCCESS;		
		break;
	    }
	    default:
		printf("Un-implemented PXE API function %d\n", pxeemu_func_nr);
		retval = PXENV_EXIT_FAILURE;
		break;
	}

	longjmp(pxeemu_entry_jbuf, retval);
pxeemu_exit:
	
	return retval - PXEEMU_EXIT_ADJ;
}

void pxeemu_console_putc(uint32_t c)
{
	v86_p->ctl = 0;
	v86_p->addr = 0x10;
	v86_p->eax = c;
	v86_p->ebx = 0x7;
	V86INT();
	return;
}

#endif /* FREEBSD_PXEEMU */
/*
 * Local variables:
 *  c-basic-offset: 8
 * End:
 */
