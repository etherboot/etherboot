#include "etherboot.h"
#include "linuxbios_tables.h"

struct meminfo meminfo;

#undef DEBUG_LINUXBIOS

/* FIXME should I use ipchksum here instead? 
 * It is the same algorithm, and reuse would save a few bytes... 
 */
static unsigned long compute_checksum(void *addr, unsigned long length)
{
	/* Assumes the start is 2 byte aligned.
	 * This isn't exactly a problem on x86 but...
	 */
	unsigned short *ptr = addr;
	unsigned long len;
	unsigned long remainder;
	unsigned long partial;
	partial = 0;
	len = length >> 1;
	remainder = len & 1;
	while(len--) {
		partial += *(ptr++);
		if (partial > 0xFFFF) 
			partial -= 0xFFFF;
	}
	if (remainder) {
		unsigned char *ptr2 = (void *)ptr;
		partial += *ptr2;
		if (partial > 0xFFFF)
			partial -= 0xFFFF;
	}
	return (~partial) & 0xFFFF;
}

static void set_base_mem_k(struct meminfo *info, unsigned mem_k)
{
	if ((mem_k <= 640) && (info->basememsize <= mem_k)) {
		info->basememsize = mem_k;
	}
}
static void set_high_mem_k(struct meminfo *info, unsigned mem_k)
{
	/* Shave off a megabyte before playing */
	if (mem_k < 1024) {
		return;
	}
	mem_k -= 1024;
	if (info->memsize <= mem_k) {
		info->memsize = mem_k;
	}
}

static void read_lb_memory(
	struct meminfo *info, struct lb_memory *mem)
{
	int i;
	int entries;
	entries = (mem->size - sizeof(*mem))/sizeof(mem->map[0]);
	for(i = 0; (i < entries); i++) {
		if (info->map_count < E820MAX) {
			info->map[info->map_count].addr = mem->map[i].start;
			info->map[info->map_count].size = mem->map[i].size;
			info->map[info->map_count].type = mem->map[i].type;
			info->map_count++;
		}
		switch(mem->map[i].type) {
		case LB_MEM_RAM:
		{
			unsigned long long high;
			unsigned long mem_k;
			high = mem->map[i].start + mem->map[i].size;
#if defined(DEBUG_LINUXBIOS)
			printf("lb: ram start: %X%X size: %X%X high: %X%X\n",
				(unsigned long)(mem->map[i].start >>32), 
				(unsigned long)(mem->map[i].start & 0xFFFFFFFF), 
				(unsigned long)(mem->map[i].size >> 32),
				(unsigned long)(mem->map[i].size  & 0xFFFFFFFF),
				(unsigned long)(high >> 32), 
				(unsigned long)(high & 0xFFFFFFFF));
#endif /* DEBUG_LINUXBIOS */
			high >>= 10;
			mem_k = high;
			if (high & 0xFFFFFFFF00000000ULL) {
				mem_k = 0xFFFFFFFF;
			}
			set_base_mem_k(info, mem_k);
			set_high_mem_k(info, mem_k);
			break;
		}
		case LB_MEM_RESERVED:
		default:
			break;
		}
	}
}


static void read_linuxbios_values(struct meminfo *info,
	struct lb_header *head)
{
	/* Read linuxbios tables... */
	struct lb_record *rec;
	void *start, *end;

	start = ((unsigned char *)head) + sizeof(*head);
	end = ((char *)start) + head->table_bytes;
	for(rec = start; ((void *)rec < end) &&
		((long)rec->size <= (end - (void *)rec)); 
		rec = (void *)(((char *)rec) + rec->size)) {
		switch(rec->tag) {
		case LB_TAG_MEMORY:
		{
			struct lb_memory *mem;
			mem = (struct lb_memory *) rec;
			read_lb_memory(info, mem); 
			break;
		}
		default: 
			break;
		};
	}
}



static unsigned long count_lb_records(void *start, unsigned long length)
{
	struct lb_record *rec;
	void *end;
	unsigned long count;
	count = 0;
	end = ((char *)start) + length;
	for(rec = start; ((void *)rec < end) &&
		((signed long)rec->size <= (end - (void *)rec)); 
		rec = (void *)(((char *)rec) + rec->size)) {
		count++;
	}
	return count;
}

static int find_lb_table(void *start, void *end, struct lb_header **result)
{
	unsigned char *ptr;
	/* For now be stupid.... */
	for(ptr = start; (void *)ptr < end; ptr += 16) {
		struct lb_header *head = (void *)ptr;
		if (	(head->signature[0] == 'L') && 
			(head->signature[1] == 'B') &&
			(head->signature[2] == 'I') &&
			(head->signature[3] == 'O') &&
			(head->header_bytes == sizeof(*head)) &&
			(compute_checksum(head, sizeof(*head)) == 0) &&
			(compute_checksum(ptr + sizeof(*head), head->table_bytes) ==
				head->table_checksum) &&
			(count_lb_records(ptr + sizeof(*head), head->table_bytes) ==
				head->table_entries)
			) {
			*result = head;
			return 1;
		}
	};
	return 0;
}

void get_memsizes(void)
{
	struct lb_header *lb_table;
	int found;
#if defined(DEBUG_LINUXBIOS)
	printf("\nSearching for linuxbios tables...\n");
#endif /* DEBUG_LINUXBIOS */
	found = 0;
	meminfo.basememsize = 0;
	meminfo.memsize = 0;
	meminfo.map_count = 0;
	/* This code is specific to linuxBIOS but could
	 * concievably be extended to work under a normal bios.
	 * but size is important...
	 */
	if (!found) {
		found = find_lb_table((void*)0x00000, (void*)0x01000, &lb_table);
	}
	if (!found) {
		found = find_lb_table((void*)0xf0000, (void*)0x100000, &lb_table);
	}
	if (found) {
		read_linuxbios_values(&meminfo, lb_table);
	}

#if defined(DEBUG_LINUXBIOS)
	printf("base_mem_k = %d high_mem_k = %d\n", 
		meminfo.basememsize, meminfo.memsize);
#endif /* DEBUG_LINUXBIOS */
	
}

