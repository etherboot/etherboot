#include "efi/efi.h"
#include "etherboot.h"
#include "elf.h"
#include "sal.h"
#include "pal.h"

#warning "Place the declaraction of __call someplace more appropriate\n"
extern EFI_STATUS __call(void *,...);

/* Keep 16M free in case EFI needs to allocate some memory. 
 * In the worst case this is only 1/8 the memory on an Itanium.
 */
#define EFI_RESERVE_LOW_PAGES  ((8*1024*1024)/EFI_PAGE_SIZE)
#define EFI_RESERVE_HIGH_PAGES ((8*1024*1024)/EFI_PAGE_SIZE)

struct console_info {
	uint16_t num_cols;
	uint16_t num_rows;
	uint16_t orig_x;
	uint16_t orig_y;
};

struct efi_mem_map {
	uint64_t	map_size;
	uint64_t	map_key;
	uint64_t	descriptor_size;
	uint32_t              descriptor_version;
	uint32_t              pad;
	EFI_MEMORY_DESCRIPTOR map[64];
};

struct efi_info {
	int flags;
#define READ_SYSTAB  1
#define READ_FPSWA   2
#define READ_MEMMAP  4	
#define READ_CONINFO 8
	EFI_SYSTEM_TABLE *systab;
	void *fpswa;
	struct efi_mem_map mem_map;
	struct console_info coninfo;
};


struct efi_info efi_info;
unsigned long io_base;

/* local globals */
static EFI_HANDLE etherboot_handle;
static EFI_BOOT_SERVICES *boot_services;
static void *mps_table;
static void *acpi20_table;
static void *smbios_table;
static void *nii_table;

/* local functions */

static EFI_STATUS efi_locate_handle(
	EFI_LOCATE_SEARCH_TYPE search_type, 
	EFI_GUID *protocol, void *search_key, 
	UINTN *buffer_size, EFI_HANDLE *buffer)
{
	if (!boot_services)
		return EFI_NOT_FOUND;
	return __call(boot_services->LocateHandle, 
		search_type, protocol, search_key, buffer_size, buffer);
}

static EFI_STATUS efi_handle_protocol(EFI_HANDLE handle, EFI_GUID *protocol, void **interface)
{
	if (!boot_services)
		return EFI_UNSUPPORTED;
	return __call(boot_services->HandleProtocol, handle, protocol, interface);
}

static EFI_STATUS efi_locate_device_path(EFI_GUID *protocol, EFI_DEVICE_PATH **device_path,
	EFI_HANDLE *device)
{
	if (!boot_services)
		return EFI_NOT_FOUND;
	return __call(boot_services->LocateDevicePath, protocol, device_path, device);
}

static EFI_STATUS efi_allocate_pages(EFI_ALLOCATE_TYPE type, EFI_MEMORY_TYPE memory_type, 
	UINTN pages, EFI_PHYSICAL_ADDRESS *memory)
{
	if (!boot_services)
		return EFI_OUT_OF_RESOURCES;
	return __call(boot_services->AllocatePages,
		type, memory_type, pages, memory);
}

static EFI_STATUS efi_free_pages(EFI_PHYSICAL_ADDRESS memory, UINTN pages)
{
	if(pages == 0)
		return EFI_SUCCESS;
	if (!boot_services)
		return EFI_INVALID_PARAMETER;
	return __call(boot_services->FreePages, memory, pages);
}

static EFI_STATUS efi_get_memory_map(UINTN *map_size, EFI_MEMORY_DESCRIPTOR *map,
	UINTN *map_key, UINTN *descriptor_size, UINT32 *descriptor_version)
{
	if (!boot_services)
		return EFI_INVALID_PARAMETER;
	return __call(boot_services->GetMemoryMap,
		map_size, map, map_key, descriptor_size, descriptor_version);
}

static EFI_STATUS efi_free_pool(void *buffer)
{
	if (!boot_services)
		return EFI_INVALID_PARAMETER;
	return __call(boot_services->FreePool, buffer);
}
static EFI_STATUS efi_stall(UINTN microseconds)
{
	if (!boot_services)
		return EFI_UNSUPPORTED;
	return __call(boot_services->Stall, microseconds);
}

static EFI_STATUS efi_set_watchdog_timer(
	UINTN timeout, UINT64 watchdog_code, UINTN data_size, CHAR16 *watchdog_data)
{
	if (!boot_services)
		return EFI_UNSUPPORTED;
	return __call(boot_services->SetWatchdogTimer,
		timeout, watchdog_code, data_size, watchdog_data);
}


static void efi_exit_boot_services(struct efi_mem_map *map)
{
	EFI_STATUS status;
	if (!boot_services)
		return;
	status = __call(boot_services->ExitBootServices, 
		etherboot_handle, map->map_key);
	if (status != EFI_SUCCESS) {
		printf("ExitBootServices failed: %lx\n", status);
	}
	boot_services = 0;
}

static void efi_free_memory(struct efi_mem_map *map)
{
	EFI_MEMORY_DESCRIPTOR *desc, *tail;
#define next_desc(desc, size) ((EFI_MEMORY_DESCRIPTOR *)(((char *)(desc)) + (size)))
	tail = next_desc(map->map, map->map_size);
	for(desc = map->map; desc < tail; desc = next_desc(desc, map->descriptor_size)) {
		EFI_STATUS status;
		EFI_PHYSICAL_ADDRESS start, end;
		UINTN pages;
		int may_free;

		start = desc->PhysicalStart;
		pages = desc->NumberOfPages;
		end = start + pages * EFI_PAGE_SIZE;


		may_free = 0;
		/* The only canidates are Loader Code and Data */
		if ((desc->Type == EfiLoaderData) ||
			(desc->Type == EfiLoaderCode))
			may_free = 1;
	
		/* Don't free anything etherboot lives in */
		if ((may_free) &&
			(start < virt_to_phys(_end)) &&
			(end > virt_to_phys(_text)))
			may_free = 0;

#if 0 /* Efi isn't especially happy when I free myself */
		if (desc->Type == EfiLoaderCode)
			may_free = 0;
#endif
		/* Continue if it is not memory we want to free */
		if (!may_free)
			continue;

		status = efi_free_pages(start, pages);
		if (status != EFI_SUCCESS) {
			printf("free_pages: %lx\n", status);
		}
	}
#undef next_desc
}

static void read_efi_mem_map(struct efi_mem_map *map)
{
	EFI_STATUS status;
	map->map_size = sizeof(map->map);
	status = efi_get_memory_map(
		&map->map_size, map->map, &map->map_key,
		&map->descriptor_size, &map->descriptor_version);
	if (status != EFI_SUCCESS) {
		printf("read_efi_mem_map failed: %lx\n", status);
		map->map_size = 0;
	}
#if 0
	if (map->descriptor_size != sizeof(map->map[0])) {
		printf("descriptor_size %ld expected %ld\n",
			efi_mem_descriptor_size, sizeof(efi_mem_map[0]));
	}
#endif
#if 0
	if (map->descriptor_version != EFI_MEMORY_DESCRIPTOR_VERSION) {
		printf("descriptor_version %d expected %d\n",
			map->descriptor_version, EFI_MEMORY_DESCRIPTOR_VERSION);
	}
#endif
}


static void efi_allocate_memory(struct efi_mem_map *map)
{
	EFI_MEMORY_DESCRIPTOR *desc, *end;
	unsigned long low_free, high_free;

#define next_desc(desc, size) ((EFI_MEMORY_DESCRIPTOR *)(((char *)(desc)) + (size)))
	end = next_desc(map->map, map->map_size);
	/* Find out how much memory is free */
	low_free = high_free = 0;
	for(desc = map->map; desc < end ; desc = next_desc(desc, map->descriptor_size)) {
		unsigned long start, middle, end;
		if (desc->Type != EfiConventionalMemory)
			continue;
		start = desc->PhysicalStart;
		end = desc->PhysicalStart + (desc->NumberOfPages*EFI_PAGE_SIZE);
		if (start < 0x100000000UL) {
			if (end > 0x100000000UL) {
				middle = 0x10000000UL;
			} else {
				middle = end;
			}
		} else {
			middle = start;
		}

		low_free += (middle - start)/EFI_PAGE_SIZE;
		high_free += (end - middle)/EFI_PAGE_SIZE;
	}
	/* O.k. Now allocate all of the conventional memory, reserving only a tiny
	 * fraction for efi.
	 */
	for(desc = map->map; desc < end ; desc = next_desc(desc, map->descriptor_size)) {
		EFI_STATUS status;
		EFI_PHYSICAL_ADDRESS address;
		UINTN pages;
		unsigned long start, middle, end;
		unsigned long low_pages, high_pages;
		if (desc->Type != EfiConventionalMemory)
			continue;
		start = desc->PhysicalStart;
		end = desc->PhysicalStart + (desc->NumberOfPages*EFI_PAGE_SIZE);
		if (start < 0x100000000UL) {
			if (end > 0x100000000UL) {
				middle = 0x10000000UL;
			} else {
				middle = end;
			}
		} else {
			middle = start;
		}
		low_pages  = (middle - start)/EFI_PAGE_SIZE;
		high_pages = (end - middle)/EFI_PAGE_SIZE;
		if (low_pages && (low_free > EFI_RESERVE_LOW_PAGES)) {
			address = start;
			pages = low_pages;
			if ((low_free - pages) < EFI_RESERVE_LOW_PAGES) {
				pages = low_free - EFI_RESERVE_LOW_PAGES;
			}
			status = efi_allocate_pages(
				AllocateAddress, EfiLoaderData, pages, &address);
			if (status != EFI_SUCCESS) {
				printf("allocate_pages @%lx for %ld pages failed: %ld\n", 
					desc->PhysicalStart, pages, status);
			}
			low_free -= pages;
		}
		if (high_pages && (high_free > EFI_RESERVE_HIGH_PAGES)) {
			address = middle;
			pages = high_pages;
			if ((high_free - pages) < EFI_RESERVE_HIGH_PAGES) {
				pages = high_free - EFI_RESERVE_HIGH_PAGES;
			}
			status = efi_allocate_pages(
				AllocateAddress, EfiLoaderData, pages, &address);
			if (status != EFI_SUCCESS) {
				printf("allocate_pages @%lx for %ld pages failed: %ld\n", 
					desc->PhysicalStart, pages, status);
			}
			high_free -= pages;
		}
	}
#undef next_desc
}

static void set_io_base(struct efi_mem_map *map)
{
	EFI_MEMORY_DESCRIPTOR *desc, *end;

	io_base = ia64_get_kr0(); /* Default to ar.kr0 */

#define next_desc(desc, size) ((EFI_MEMORY_DESCRIPTOR *)(((char *)(desc)) + (size)))
	end = next_desc(map->map, map->map_size);

	for(desc = map->map; desc < end ; desc = next_desc(desc, map->descriptor_size)) {
		if (desc->Type == EfiMemoryMappedIOPortSpace) {
			io_base = desc->PhysicalStart;
			break;
		}
	}
#undef next_desc
}

static void efi_get_coninfo(struct console_info *info)
{
	SIMPLE_TEXT_OUTPUT_INTERFACE *conout;
	EFI_STATUS status;
	UINTN cols, rows;

	/* Initialize with some silly safe values */
	info->num_cols = 80;
	info->num_rows = 24;
	info->orig_x   = 0;
	info->orig_y   = 0;

	status = EFI_UNSUPPORTED;
	if (boot_services) {
		conout = efi_info.systab->ConOut;
		status = __call(conout->QueryMode, conout, conout->Mode->Mode, &cols, &rows);
		if (status) {
			printf("QueryMode Failed cannout get console parameters: %ld\n", status);
		} else {
			info->num_cols = cols;
			info->num_rows = rows;
			info->orig_x   = conout->Mode->CursorColumn;
			info->orig_y   = conout->Mode->CursorRow;
		}
	}
}

static void *efi_get_fpswa(void)
{
	static EFI_GUID fpswa_protocol = FPSWA_PROTOCOL;
	EFI_STATUS status;
	EFI_HANDLE fpswa_handle;
	UINTN devices;
	void *result;

	/* The FPSWA is the Floating Point Software Assist driver,
	 * to some extent it makes sense but it has one large flaw.
	 * It fails to install an EFI Configuration table, so the
	 * OS does not need assistance from the bootloader to find it.
	 */
	devices = sizeof(fpswa_handle);
	status = efi_locate_handle(
		ByProtocol, &fpswa_protocol, 0, &devices, &fpswa_handle);
	if (status != EFI_SUCCESS)
		return 0;
	
	status = efi_handle_protocol(
		fpswa_handle, &fpswa_protocol, &result);
	if (status != EFI_SUCCESS)
		return 0;

	return result;
}


/* Exported functions */


void arch_main(struct Elf_Bhdr *bhdr)
{
	EFI_STATUS status;
	unsigned char *note, *end;

	memset(&efi_info, 0, sizeof(efi_info));
	note = ((char *)bhdr) + sizeof(*bhdr);
	end  = ((char *)bhdr) + bhdr->b_size;
	if (bhdr->b_signature != 0x0E1FB007) {
		printf("Bad bhdr(%lx) signature(%x)!\n",
			(unsigned long) bhdr, bhdr->b_signature);
		note = end = 0;
	}
	while(note < end) {
		Elf_Nhdr *hdr;
		unsigned char *n_name, *n_desc, *next;
		hdr = (Elf_Nhdr *)note;
		n_name = note + sizeof(*hdr);
		n_desc = n_name + ((hdr->n_namesz + 3) & ~3);
		next = n_desc + ((hdr->n_descsz + 3) & ~3);
		if (next > end) 
			break;
#if 0
		printf("n_type: %x n_name(%d): n_desc(%d): \n", 
			hdr->n_type, hdr->n_namesz, hdr->n_descsz);
#endif
		if ((hdr->n_namesz == 10) &&
			(memcmp(n_name, "Etherboot", 10) == 0)) {
			switch(hdr->n_type) {
			case EB_IA64_IMAGE_HANDLE:
			{
				uint64_t *handlep = (void *)n_desc;
				etherboot_handle = (EFI_HANDLE)(*handlep);
				break;
			}
			case EB_IA64_SYSTAB:
			{
				uint64_t *systabp = (void *)n_desc;
				efi_info.systab = (void *)(*systabp);
				efi_info.flags |= READ_SYSTAB;
				break;
			}
			case EB_IA64_FPSWA:
			{
				uint64_t*fpswap = (void *)n_desc;
				efi_info.fpswa = (void *)(*fpswap);
				efi_info.flags |= READ_FPSWA;
				break;
			}
			case EB_IA64_CONINFO:
			{
				struct console_info *coninfop = (void *)n_desc;
				efi_info.coninfo = *coninfop;
				efi_info.flags |= READ_CONINFO;
				break;
			}
			case EB_IA64_MEMMAP:
			{
				struct efi_mem_map *mem_mapp = (void *)n_desc;
				efi_info.mem_map = *mem_mapp;
				efi_info.flags |= READ_MEMMAP;
				break;
			}
			default:
				break;
			}
		}
		note = next;
	}
	if (!(efi_info.flags & READ_SYSTAB)) {
		printf("No EFI systab\n");
		return;
	}
	
	/* If I have an efi memory map assume ExitBootServices has been called.
	 */
#warning "FIXME see if there is a better test for boot services still being active "
	printf("FIXME Develop a better test for boot services still being active\n");
	if (!(efi_info.flags & READ_MEMMAP)) {
		boot_services = efi_info.systab->BootServices;
	}

	if (!(efi_info.flags & READ_CONINFO)) {
		efi_info.flags |= READ_CONINFO;
		efi_get_coninfo(&efi_info.coninfo);
	}
	if (!(efi_info.flags & READ_FPSWA)) {
		efi_info.flags |= READ_FPSWA;
		efi_info.fpswa = efi_get_fpswa();
	}
	if (!(efi_info.flags & READ_MEMMAP)) {
		efi_info.flags |= READ_MEMMAP;
		read_efi_mem_map(&efi_info.mem_map);
		/* Allocate all of the memory efi can spare */
		efi_allocate_memory(&efi_info.mem_map);
		/* Now refresh the memory map */
		read_efi_mem_map(&efi_info.mem_map);
	}
	/* Get the io_base for legacy i/o */
	set_io_base(&efi_info.mem_map);

	/* Attempt to disable the watchdog timer.. 
	 * Nothing useful can be done if this fails, so ignore the return code.
	 */
	status = efi_set_watchdog_timer(0, 1, 0, 0);

	if (efi_info.systab) {
		static const EFI_GUID mps_table_guid = MPS_TABLE_GUID;
		static const EFI_GUID acpi20_table_guid = ACPI_20_TABLE_GUID;
		static const EFI_GUID smbios_table_guid = SMBIOS_TABLE_GUID;
		static const EFI_GUID sal_system_table_guid = SAL_SYSTEM_TABLE_GUID;
		static const EFI_GUID nii_table_guid = EFI_NETWORK_INTERFACE_IDENTIFIER_PROTOCOL;
		EFI_SYSTEM_TABLE *systab;
		unsigned i;
		systab = efi_info.systab;
		for(i = 0; i < systab->NumberOfTableEntries; i++) {
			EFI_GUID *guid;
			void *table = systab->ConfigurationTable[i].VendorTable;
			guid = &systab->ConfigurationTable[i].VendorGuid;

#if 0
			printf("GUID: %x-%hx-%hx-%hhx-%hhx-%hhx-%hhx-%hhx-%hhx-%hhx-%hhx Table: %lx\n",
				guid->Data1, guid->Data2, guid->Data3,
				guid->Data4[0],	guid->Data4[1], guid->Data4[2],	guid->Data4[3],
				guid->Data4[4],	guid->Data4[5],	guid->Data4[6],	guid->Data4[7],
				table);
#endif

			if (memcmp(guid, &mps_table_guid, 16) == 0) {
				mps_table = table;
			}
			if (memcmp(guid, &acpi20_table_guid, 16) == 0) {
				acpi20_table = table;
			}
			if (memcmp(guid, &smbios_table_guid, 16) == 0) {
				smbios_table = table;
			}
			if (memcmp(guid, &sal_system_table_guid, 16) == 0) {
				parse_sal_system_table(table);
			}
			if (memcmp(guid, &nii_table_guid, 16) == 0) {
				nii_table = table;
			}
		}
	}
}

void arch_on_exit(int status)
{
	if (!boot_services)
		return;
	read_efi_mem_map(&efi_info.mem_map);
	efi_free_memory(&efi_info.mem_map);
}

void arch_relocate_to(unsigned long addr)
{
	EFI_PHYSICAL_ADDRESS address, end;
	UINTN pages;
	EFI_STATUS status;
	
	if (!boot_services)
		return;

	/* Find the efi pages where the new etherboot will sit */
	address = addr & ~(EFI_PAGE_SIZE -1);
	end = (addr + (_end - _text) + EFI_PAGE_SIZE -1) & ~EFI_PAGE_SIZE;
	pages = (end - address)/EFI_PAGE_SIZE;

#if 0
	printf("efi_relocate_to: %lx-%lx %ld pages\n",
		address, end, pages);
#endif
#if 0
	/* Update the LOADED_IMAGE_PROTOCOL structure */
	if (etherboot_image) {
		etherboot_image->ImageBase = (void *)address;
		etherboot_image->ImageSize = end - address;

#if 1
		printf("efi etherboot @[%lx - %lx)\n",
			address, end);
#endif
	}
#endif

	/* Reallocate the memory for the new copy of etherboot as LoaderCode */
	status = efi_free_pages(address, pages);
	if (status != EFI_SUCCESS) {
		printf("efi_free_pages failed!: %lx\n", status);
		return;
	}
	status = efi_allocate_pages(AllocateAddress, EfiLoaderCode, pages, &address);
	if (status != EFI_SUCCESS) {
		printf("efi_allocate_pages failed! %lx\n", status);
		return;
	}
}


struct meminfo meminfo;
void get_memsizes(void)
{
	EFI_MEMORY_DESCRIPTOR *desc, *end;
	struct efi_mem_map *map;
#define next_desc(desc, size) ((EFI_MEMORY_DESCRIPTOR *)(((char *)(desc)) + (size)))

	map = &efi_info.mem_map;
	end = next_desc(map->map, map->map_size);

	meminfo.map_count = 0;
	for(desc = map->map; desc < end ; desc = next_desc(desc, map->descriptor_size)) {
		uint64_t start, size, end;
		unsigned long mem_k;
		
		start = desc->PhysicalStart;
		size  = desc->NumberOfPages*EFI_PAGE_SIZE;
		end   = start + size;

		if ((desc->Type != EfiLoaderCode) &&
			(desc->Type != EfiLoaderData)) {
			continue;
		}

		meminfo.map[meminfo.map_count].addr = start;
		meminfo.map[meminfo.map_count].size = size;
		meminfo.map[meminfo.map_count].type = E820_RAM;
		meminfo.map_count++;

		end >>= 10;
		mem_k = end;
		if (end & 0xFFFFFFFF00000000ULL) {
			mem_k = 0xFFFFFFFF;
		}
		/* Set the base basememsize */
		if ((mem_k <= 640) && (meminfo.basememsize <= mem_k)) {
			meminfo.basememsize = mem_k;
		}
		/* Set the total memsize */
		if ((mem_k >= 1024) && (meminfo.memsize <= (mem_k - 1024))) {
			meminfo.memsize = mem_k - 1024;
		}
		if (meminfo.map_count == E820MAX)
			break;
	}
#undef next_desc
}


#define NAME "Etherboot"
#define FIRMWARE "EFI"

#define SZ(X) ((sizeof(X)+3) & ~3)
#define CP(D,S) (memcpy(&(D), &(S), sizeof(S)))


struct elf_notes {
	/* CAREFUL this structure is carefully arranged to avoid
	 * alignment problems.
	 */
	/* The note header */
	struct Elf_Bhdr hdr;
	
	/* First the Fixed sized entries that must be well aligned */

	/* Insert a nop record so the next record is 64bit aligned */
	Elf_Nhdr nf0;

	/* Pointer to bootp data */
	Elf_Nhdr nf1;
	char     nf1_name[SZ(EB_PARAM_NOTE)];
	uint64_t nf1_bootp_data;

	/* Pointer to ELF header */
	Elf_Nhdr nf2;
	char     nf2_name[SZ(EB_PARAM_NOTE)];
	uint64_t nf2_header;

	/* The EFI systab pointer */
	Elf_Nhdr nf3;
	char     nf3_name[SZ(EB_PARAM_NOTE)];
	uint64_t nf3_systab;

	/* The FPSWA pointer */
	Elf_Nhdr nf4;
	char     nf4_name[SZ(EB_PARAM_NOTE)];
	uint64_t nf4_fpswa;

	/* The memory map */
	Elf_Nhdr nf5;
	char     nf5_name[SZ(EB_PARAM_NOTE)];
	struct efi_mem_map nf5_map;

	/* The console info, silly but elilo passes it... */
	Elf_Nhdr nf6;
	char     nf6_name[SZ(EB_PARAM_NOTE)];
	struct console_info nf6_coninfo;

	/* Then the variable sized data string data where alignment does not matter */

	/* The bootloader name */
	Elf_Nhdr nv1;
	char     nv1_desc[SZ(NAME)];
	/* The bootloader version */
	Elf_Nhdr nv2;
	char     nv2_desc[SZ(VERSION)];
	/* The firmware type */
	Elf_Nhdr nv3;
	char     nv3_desc[SZ(FIRMWARE)];
	/* Name of the loaded image */
	Elf_Nhdr nv4;
	char	nv4_loaded_image[128];
	/* An empty command line */
	Elf_Nhdr nv5;
	char     nv5_cmdline[SZ("")];
};

#define ELF_NOTE_COUNT	(6+5)

static struct elf_notes notes;
struct Elf_Bhdr *prepare_boot_params(void *header)
{
	/* Shutdown the boot services */
	if (boot_services) {
		efi_get_coninfo(&efi_info.coninfo);
		read_efi_mem_map(&efi_info.mem_map);
		efi_exit_boot_services(&efi_info.mem_map);
	}

	/* read_efi_mem_map should come last. */
	notes.nf5.n_namesz = sizeof(EB_PARAM_NOTE);
	notes.nf5.n_descsz = sizeof(notes.nf5_map);
	notes.nf5.n_type   = EB_IA64_MEMMAP;
	CP(notes.nf5_name,   EB_PARAM_NOTE);
	read_efi_mem_map(&notes.nf5_map);

	memset(&notes, 0, sizeof(notes));
	notes.hdr.b_signature = 0x0E1FB007;
	notes.hdr.b_size      = sizeof(notes);
	notes.hdr.b_checksum  = 0;
	notes.hdr.b_records   = ELF_NOTE_COUNT;

	/* Initialize the fixed length entries. */

	/* Align the fixed length entries to a 64bit boundary */
	notes.nf0.n_namesz = 0;
	notes.nf0.n_descsz = 0;
	notes.nf0.n_type   = EBN_NOP;

	notes.nf1.n_namesz = sizeof(EB_PARAM_NOTE);
	notes.nf1.n_descsz = sizeof(notes.nf1_bootp_data);
	notes.nf1.n_type   = EB_BOOTP_DATA;
	CP(notes.nf1_name,   EB_PARAM_NOTE);
	notes.nf1_bootp_data = virt_to_phys(BOOTP_DATA_ADDR);

	notes.nf2.n_namesz = sizeof(EB_PARAM_NOTE);
	notes.nf2.n_descsz = sizeof(notes.nf2_header);
	notes.nf2.n_type   = EB_HEADER;
	CP(notes.nf2_name,   EB_PARAM_NOTE);
	notes.nf2_header   = virt_to_phys(header);

	notes.nf3.n_namesz = sizeof(EB_PARAM_NOTE);
	notes.nf3.n_descsz = sizeof(notes.nf3_systab);
	notes.nf3.n_type   = EB_IA64_SYSTAB;
	CP(notes.nf3_name,   EB_PARAM_NOTE);
	notes.nf3_systab   = (unsigned long)efi_info.systab;

	notes.nf4.n_namesz = sizeof(EB_PARAM_NOTE);
	notes.nf4.n_descsz = sizeof(notes.nf4_fpswa);
	notes.nf4.n_type   = EB_IA64_FPSWA;
	CP(notes.nf4_name,   EB_PARAM_NOTE);
	notes.nf4_fpswa = (unsigned long)efi_info.fpswa;

	notes.nf5.n_namesz = sizeof(EB_PARAM_NOTE);
	notes.nf5.n_descsz = sizeof(notes.nf5_map);
	notes.nf5.n_type   = EB_IA64_MEMMAP;
	CP(notes.nf5_name,   EB_PARAM_NOTE);
	notes.nf5_map      = efi_info.mem_map;

	notes.nf6.n_namesz = sizeof(EB_PARAM_NOTE);
	notes.nf6.n_descsz = sizeof(notes.nf6_coninfo);
	notes.nf6.n_type   = EB_IA64_CONINFO;
	CP(notes.nf6_name,   EB_PARAM_NOTE);
	notes.nf6_coninfo  = efi_info.coninfo;

	/* Initialize the variable length entries */
	notes.nv1.n_namesz = 0;
	notes.nv1.n_descsz = sizeof(NAME);
	notes.nv1.n_type   = EBN_BOOTLOADER_NAME;
	CP(notes.nv1_desc,   NAME);

	notes.nv2.n_namesz = 0;
	notes.nv2.n_descsz = sizeof(VERSION);
	notes.nv2.n_type   = EBN_BOOTLOADER_VERSION;
	CP(notes.nv2_desc,   VERSION);

	notes.nv3.n_namesz = 0;
	notes.nv3.n_descsz = sizeof(FIRMWARE);
	notes.nv3.n_type   = EBN_FIRMWARE_TYPE;
	CP(notes.nv3_desc,   FIRMWARE);

	/* Attempt to pass the name of the loaded image */
	notes.nv4.n_namesz = 0;
	notes.nv4.n_descsz = sizeof(notes.nv4_loaded_image);
	notes.nv4.n_type   = EBN_LOADED_IMAGE;
	memcpy(&notes.nv4_loaded_image, KERNEL_BUF, sizeof(notes.nv4_loaded_image));

	/* Pass an empty command line for now */
	notes.nv5.n_namesz = 0;
	notes.nv5.n_descsz = sizeof("");
	notes.nv5.n_type   = EBN_COMMAND_LINE;
	CP(notes.nv5_cmdline,   "");

	notes.hdr.b_checksum = ipchksum(&notes, sizeof(notes));
	/* Like UDP invert a 0 checksum to show that a checksum is present */
	if (notes.hdr.b_checksum == 0) {
		notes.hdr.b_checksum = 0xffff;
	}

	return &notes.hdr;
}

int elf_start(unsigned long machine __unused, unsigned long entry, unsigned long params)
{
	struct elf_notes *notes;
	/* Since we can do both be polite and also pass the linux
	 * ia64_boot_param table.
	 */
	static struct ia64_boot_param {
		uint64_t command_line;		/* physical address of command line arguments */
		uint64_t efi_systab;		/* physical address of EFI system table */
		uint64_t efi_memmap;		/* physical address of EFI memory map */
		uint64_t efi_memmap_size;		/* size of EFI memory map */
		uint64_t efi_memdesc_size;		/* size of an EFI memory map descriptor */
		uint32_t efi_memdesc_version;	/* memory descriptor version */
		struct {
			uint16_t num_cols;	/* number of columns on console output device */
			uint16_t num_rows;	/* number of rows on console output device */
			uint16_t orig_x;	/* cursor's x position */
			uint16_t orig_y;	/* cursor's y position */
		} console_info;
		uint64_t fpswa;		/* physical address of the fpswa interface */
		uint64_t initrd_start;
		uint64_t initrd_size;
	} bp;

	notes = phys_to_virt(params);
	/* If I don't have notes don't attempt to start the image */
	if (notes == 0) {
		return -2;
	}

	#warning "Second pass at elf_start this is not the final version"
	printf("FIXME: This is just the second pass at elf_start not the final version: %s:%d\n", 
		__FILE__, __LINE__);


	bp.command_line          = (unsigned long)&notes->nv5_cmdline;
	bp.efi_systab            = notes->nf3_systab;
	bp.efi_memmap            = (unsigned long)&notes->nf5_map.map;
	bp.efi_memmap_size       = notes->nf5_map.map_size;
	bp.efi_memdesc_size      = notes->nf5_map.descriptor_size;
	bp.efi_memdesc_version   = notes->nf5_map.descriptor_version;
	bp.console_info.num_cols = notes->nf6_coninfo.num_cols;
	bp.console_info.num_rows = notes->nf6_coninfo.num_rows;
	bp.console_info.orig_x   = notes->nf6_coninfo.orig_x;
	bp.console_info.orig_y   = notes->nf6_coninfo.orig_y;
	bp.fpswa                 = notes->nf4_fpswa;
	bp.initrd_start          = 0;
	bp.initrd_size           = 0;


	asm volatile(
		";;\n\t"
		"mov r28=%1\n\t"
		"mov out0=%2\n\t"
		"br.call.sptk.few rp=%0\n\t"
		:: "b"(entry), "r"(&bp) 
		,"r"(params)
		);
	printf("The kernel returned?? FIXME get it's return value....\n");
#warning "FINISHME get the return value from the loaded image";
	return -1;
}
