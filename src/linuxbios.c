#ifdef LINUXBIOS

#include "etherboot.h"
#include "linuxbios_tables.h"

struct meminfo meminfo;

#undef DEBUG_LINUXBIOS

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
			unsigned long long end;
			unsigned long mem_k;
			end = mem->map[i].start + mem->map[i].size;
#if defined(DEBUG_LINUXBIOS)
			printf("lb: %X%X - %X%X (ram)\n",
				(unsigned long)(mem->map[i].start >>32), 
				(unsigned long)(mem->map[i].start & 0xFFFFFFFF), 
				(unsigned long)(end >> 32), 
				(unsigned long)(end & 0xFFFFFFFF));
#endif /* DEBUG_LINUXBIOS */
			end >>= 10;
			mem_k = end;
			if (end & 0xFFFFFFFF00000000ULL) {
				mem_k = 0xFFFFFFFF;
			}
			set_base_mem_k(info, mem_k);
			set_high_mem_k(info, mem_k);
			break;
		}
		case LB_MEM_RESERVED:
		default:
#if defined(DEBUG_LINUXBIOS)
		{
			unsigned long long end;
			end = mem->map[i].start + mem->map[i].size;
			printf("lb: %X%X - %X%X (reserved)\n",
				(unsigned long)(mem->map[i].start >>32), 
				(unsigned long)(mem->map[i].start & 0xFFFFFFFF), 
				(unsigned long)(end >> 32), 
				(unsigned long)(end & 0xFFFFFFFF));
		}
#endif /* DEBUG_LINUXBIOS */
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
		struct lb_header *head = (struct lb_header *)ptr;
		if (	(head->signature[0] != 'L') || 
			(head->signature[1] != 'B') ||
			(head->signature[2] != 'I') ||
			(head->signature[3] != 'O')) {
			continue;
		}
		if (head->header_bytes != sizeof(*head))
			continue;
#if defined(DEBUG_LINUXBIOS)
		printf("Found canidate at: %X\n", (unsigned long)head);
#endif
		if (ipchksum((uint16_t *)head, sizeof(*head)) != 0) 
			continue;
#if defined(DEBUG_LINUXBIOS)
		printf("header checksum o.k.\n");
#endif
		if (ipchksum((uint16_t *)(ptr + sizeof(*head)), head->table_bytes) !=
			head->table_checksum) {
			continue;
		}
#if defined(DEBUG_LINUXBIOS)
		printf("table checksum o.k.\n");
#endif
		if (count_lb_records(ptr + sizeof(*head), head->table_bytes) !=
			head->table_entries) {
			continue;
		}
#if defined(DEBUG_LINUXBIOS)
		printf("record count o.k.\n");
#endif
		*result = head;
		return 1;
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
		found = find_lb_table(phys_to_virt(0x00000), phys_to_virt(0x01000), &lb_table);
	}
	if (!found) {
		found = find_lb_table(phys_to_virt(0xf0000), phys_to_virt(0x100000), &lb_table);
	}
	if (found) {
#if defined (DEBUG_LINUXBIOS)
		printf("Found LinuxBIOS table at: %X\n", (unsigned long)lb_table);
#endif
		read_linuxbios_values(&meminfo, lb_table);
	}

#if defined(DEBUG_LINUXBIOS)
	printf("base_mem_k = %d high_mem_k = %d\n", 
		meminfo.basememsize, meminfo.memsize);
#endif /* DEBUG_LINUXBIOS */
	
}

#endif /* LINUXBIOS */
