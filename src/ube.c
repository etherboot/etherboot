#include "etherboot.h"
#include "uniform_boot.h"

static struct bootinfo {
	unsigned base_mem_k;
	unsigned high_mem_k;
} bootinfo;



static unsigned long uniform_boot_compute_header_checksum(
	struct uniform_boot_header *header)
{
	unsigned short *ptr;
	unsigned long sum;
	unsigned long len;
	/* compute an ip style checksum on the header */
	sum = 0;
	len = header->header_bytes >> 1;
	ptr = (void *)header;
	while (len--) {
		sum += *(ptr++);
		if (sum > 0xFFFF)
			sum -= 0xFFFF;
	}
	return (~sum) & 0xFFFF;
}

static void set_base_mem_k(struct bootinfo *info, unsigned mem_k)
{
	if ((mem_k <= 640) && (info->base_mem_k <= mem_k)) {
		info->base_mem_k = mem_k;
	}
}
static void set_high_mem_k(struct bootinfo *info, unsigned mem_k)
{
	/* Shave off a megabyte before playing */
	if (mem_k < 1024) {
		return;
	}
	mem_k -= 1024;
	if (info->high_mem_k <= mem_k) {
		info->high_mem_k = mem_k;
	}
}
static void read_uniform_boot_memory(
	struct bootinfo *info, struct ube_memory *mem)
{
	int i;
	int entries;
	entries = (mem->size - sizeof(*mem))/sizeof(mem->map[0]);
	for(i = 0; (i < entries); i++) {
		switch(mem->map[i].type) {
		case UBE_MEM_RAM:
		{
			unsigned long long high;
			unsigned long mem_k;
			high = mem->map[i].start + mem->map[i].size;
#if defined(DEBUG_UBE)
			printf("ube: ram start: %X%X size: %X%X high: %X%X\n",
				(unsigned long)(mem->map[i].start >>32), 
				(unsigned long)(mem->map[i].start & 0xFFFFFFFF), 
				(unsigned long)(mem->map[i].size >> 32),
				(unsigned long)(mem->map[i].size  & 0xFFFFFFFF),
				(unsigned long)(high >> 32), 
				(unsigned long)(high & 0xFFFFFFFF));
#endif /* DEBUG_UBE */
			high >>= 10;
			mem_k = high;
			if (high & 0xFFFFFFFF00000000ULL) {
				mem_k = 0xFFFFFFFF;
			}
			set_base_mem_k(info, mem_k);
			set_high_mem_k(info, mem_k);
			break;
		}
		case UBE_MEM_ACPI:
			break;
		case UBE_MEM_NVS:
			break;
		case UBE_MEM_RESERVED:
		default:
			break;
		}
	}
}


static void read_uniform_boot_data(struct bootinfo *info,
	struct uniform_boot_header *header)
{
	/* Uniform boot environment */
	unsigned long env_bytes;
	char *env;
	unsigned long checksum;
	checksum = uniform_boot_compute_header_checksum(header);
	if (checksum != 0) {
		printf("Bad uniform boot header checksum!\n");
	}
	if (header->arg_bytes) {
		/* Ignore the command line I am passed */
	}
	env = (void *)(header->env);
	env_bytes = header->env_bytes;
	while(env_bytes) {
		struct ube_record *record;
		unsigned long mem_k;
		record = (void *)env;
		if (record->tag == UBE_TAG_MEMORY) {
			read_uniform_boot_memory(info, (void *)record);
		}
		env += record->size;
		env_bytes -= record->size;
	}
}

static void initialize_bootinfo(void)
{
	extern unsigned long initeax;
	extern unsigned long initebx;
	static int initialized = 0;
	unsigned long type;
	void *ptr;
	if (initialized) {
		return;
	}
	initialized = 1;
#if defined(DEBUG_UBE)
	printf("\nReading bootinfo initeax = %X initebx = %X\n",
		initeax, initebx);
#endif /* DEBUG_UBE */
	type = initeax;
	ptr = (void *)initebx;
	bootinfo.base_mem_k = 0;
	bootinfo.high_mem_k = 0;
	if (type == 0x0A11B007) {
		read_uniform_boot_data(&bootinfo, ptr);
	}
	if (bootinfo.base_mem_k == 0) {
		printf("No base memory found assuming 640K\n");
		bootinfo.base_mem_k = 640;
	}
#if defined(DEBUG_UBE)
	printf("base_mem_k = %d high_mem_k = %d\n", 
		bootinfo.base_mem_k, bootinfo.high_mem_k);
#endif /* DEBUG_UBE */
}
unsigned short basememsize(void)
{
	initialize_bootinfo();
	return bootinfo.base_mem_k;
}

unsigned int memsize(void)
{
	initialize_bootinfo();
	return bootinfo.high_mem_k;
}

