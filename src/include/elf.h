#ifndef ELF_H
#define ELF_H

#define EI_NIDENT	16	/* Size of e_ident array. */

/* Values for e_type. */
#define ET_NONE		0	/* No file type */
#define ET_REL		1	/* Relocatable file */
#define ET_EXEC		2	/* Executable file */
#define ET_DYN		3	/* Shared object file */
#define ET_CORE		4	/* Core file */

/* Values for e_machine (incomplete). */
#define EM_386		3	/* Intel 80386+ */

/* Values for p_type. */
#define PT_NULL		0	/* Unused entry. */
#define PT_LOAD		1	/* Loadable segment. */
#define PT_DYNAMIC	2	/* Dynamic linking information segment. */
#define PT_INTERP	3	/* Pathname of interpreter. */
#define PT_NOTE		4	/* Auxiliary information. */
#define PT_SHLIB	5	/* Reserved (not used). */
#define PT_PHDR		6	/* Location of program header itself. */

/* Values for p_flags. */
#define PF_X		0x1	/* Executable. */
#define PF_W		0x2	/* Writable. */
#define PF_R		0x4	/* Readable. */


#define	ELF_PROGRAM_RETURNS_BIT	0x8000000	/* e_flags bit 31 */

#define EI_MAG0		0
#define ELFMAG0		0x7f

#define EI_MAG1		1
#define ELFMAG1		'E'

#define EI_MAG2		2
#define ELFMAG2		'L'

#define EI_MAG3		3
#define ELFMAG3		'F'

#define ELFMAG		"\177ELF"

#define EI_CLASS	4	/* File class byte index */
#define ELFCLASSNONE	0	/* Invalid class */
#define ELFCLASS32	1	/* 32-bit objects */
#define ELFCLASS64	2	/* 64-bit objects */

#define EI_DATA		5	/* Data encodeing byte index */
#define ELFDATANONE	0	/* Invalid data encoding */
#define ELFDATA2LSB	1	/* 2's complement little endian */
#define ELFDATA2MSB	2	/* 2's complement big endian */

#define EI_VERSION	6	/* File version byte index */
				/* Value must be EV_CURRENT */

#define EV_NONE		0	/* Invalid ELF Version */
#define EV_CURRENT	1	/* Current version */

#define ELF32_PHDR_SIZE (8*4)	/* Size of an elf program header */

#ifndef ASSEMBLY
/*
 * ELF definitions common to all 32-bit architectures.
 */

typedef uint32_t	Elf32_Addr;
typedef uint16_t	Elf32_Half;
typedef uint32_t	Elf32_Off;
typedef int32_t		Elf32_Sword;
typedef uint32_t	Elf32_Word;
typedef uint32_t	Elf32_Size;

typedef uint64_t	Elf64_Addr;
typedef uint16_t	Elf64_Half;
typedef uint64_t	Elf64_Off;
typedef int32_t		Elf64_Sword;
typedef uint32_t	Elf64_Word;
typedef uint64_t	Elf64_Size;

/*
 * ELF header.
 */
typedef struct {
	unsigned char	e_ident[EI_NIDENT];	/* File identification. */
	Elf32_Half	e_type;		/* File type. */
	Elf32_Half	e_machine;	/* Machine architecture. */
	Elf32_Word	e_version;	/* ELF format version. */
	Elf32_Addr	e_entry;	/* Entry point. */
	Elf32_Off	e_phoff;	/* Program header file offset. */
	Elf32_Off	e_shoff;	/* Section header file offset. */
	Elf32_Word	e_flags;	/* Architecture-specific flags. */
	Elf32_Half	e_ehsize;	/* Size of ELF header in bytes. */
	Elf32_Half	e_phentsize;	/* Size of program header entry. */
	Elf32_Half	e_phnum;	/* Number of program header entries. */
	Elf32_Half	e_shentsize;	/* Size of section header entry. */
	Elf32_Half	e_shnum;	/* Number of section header entries. */
	Elf32_Half	e_shstrndx;	/* Section name strings section. */
} Elf32_Ehdr;

typedef struct {
	unsigned char	e_ident[EI_NIDENT];	/* File identification. */
	Elf64_Half	e_type;		/* File type. */
	Elf64_Half	e_machine;	/* Machine architecture. */
	Elf64_Word	e_version;	/* ELF format version. */
	Elf64_Addr	e_entry;	/* Entry point. */
	Elf64_Off	e_phoff;	/* Program header file offset. */
	Elf64_Off	e_shoff;	/* Section header file offset. */
	Elf64_Word	e_flags;	/* Architecture-specific flags. */
	Elf64_Half	e_ehsize;	/* Size of ELF header in bytes. */
	Elf64_Half	e_phentsize;	/* Size of program header entry. */
	Elf64_Half	e_phnum;	/* Number of program header entries. */
	Elf64_Half	e_shentsize;	/* Size of section header entry. */
	Elf64_Half	e_shnum;	/* Number of section header entries. */
	Elf64_Half	e_shstrndx;	/* Section name strings section. */
} Elf64_Ehdr;

/*
 * Program header.
 */
typedef struct {
	Elf32_Word	p_type;		/* Entry type. */
	Elf32_Off	p_offset;	/* File offset of contents. */
	Elf32_Addr	p_vaddr;	/* Virtual address (not used). */
	Elf32_Addr	p_paddr;	/* Physical address. */
	Elf32_Size	p_filesz;	/* Size of contents in file. */
	Elf32_Size	p_memsz;	/* Size of contents in memory. */
	Elf32_Word	p_flags;	/* Access permission flags. */
	Elf32_Size	p_align;	/* Alignment in memory and file. */
} Elf32_Phdr;

typedef struct {
	Elf64_Word	p_type;		/* Entry type. */
	Elf64_Word	p_flags;	/* Access permission flags. */
	Elf64_Off	p_offset;	/* File offset of contents. */
	Elf64_Addr	p_vaddr;	/* Virtual address (not used). */
	Elf64_Addr	p_paddr;	/* Physical address. */
	Elf64_Size	p_filesz;	/* Size of contents in file. */
	Elf64_Size	p_memsz;	/* Size of contents in memory. */
	Elf64_Size	p_align;	/* Alignment in memory and file. */
} Elf64_Phdr;

/* 
 * ELF Notes.
 */
typedef struct {
	Elf32_Word n_namesz;
	Elf32_Word n_descsz;
	Elf32_Word n_type;
} Elf_Nhdr;

#endif /* ASSEMBLY */

/* Standardized Elf image notes for booting... The name for all of these is ELFBoot */

#define ELF_NOTE_BOOT		"ELFBoot"

#define EIN_PROGRAM_NAME	0x00000001
/* The program in this ELF file */
#define EIN_PROGRAM_VERSION	0x00000002
/* The version of the program in this ELF file */
#define EIN_PROGRAM_CHECKSUM	0x00000003
/* ip style checksum of the memory image. */


/* ELF Defines for the current architecture */
#define	EM_CURRENT	EM_386
#define ELFDATA_CURRENT	ELFDATA2LSB

#ifdef  IMAGE_FREEBSD
/*
 * FreeBSD has this rather strange "feature" of its design.
 * At some point in its evolution, FreeBSD started to rely
 * externally on private/static/debug internal symbol information.
 * That is, some of the interfaces that software uses to access
 * and work with the FreeBSD kernel are made available not
 * via the shared library symbol information (the .DYNAMIC section)
 * but rather the debug symbols.  This means that any symbol, not
 * just publicly defined symbols can be (and are) used by system
 * tools to make the system work.  (such as top, swapinfo, swapon,
 * etc)
 *
 * Even worse, however, is the fact that standard ELF loaders do
 * not know how to load the symbols since they are not within
 * an ELF PT_LOAD section.  The kernel needs these symbols to
 * operate so the following changes/additions to the boot
 * loading of EtherBoot have been made to get the kernel to load.
 * All of the changes are within IMAGE_FREEBSD such that the
 * extra/changed code only compiles when FREEBSD support is
 * enabled.
 */

/*
 * Section header for FreeBSD (debug symbol kludge!) support
 */
typedef struct {
	Elf32_Word	sh_name;	/* Section name (index into the
					   section header string table). */
	Elf32_Word	sh_type;	/* Section type. */
	Elf32_Word	sh_flags;	/* Section flags. */
	Elf32_Addr	sh_addr;	/* Address in memory image. */
	Elf32_Off	sh_offset;	/* Offset in file. */
	Elf32_Size	sh_size;	/* Size in bytes. */
	Elf32_Word	sh_link;	/* Index of a related section. */
	Elf32_Word	sh_info;	/* Depends on section type. */
	Elf32_Size	sh_addralign;	/* Alignment in bytes. */
	Elf32_Size	sh_entsize;	/* Size of each entry in section. */
} Elf32_Shdr;

/* sh_type */
#define SHT_SYMTAB	2		/* symbol table section */
#define SHT_STRTAB	3		/* string table section */

/*
 * Module information subtypes (for the metadata that we need to build)
 */
#define MODINFO_END		0x0000		/* End of list */
#define MODINFO_NAME		0x0001		/* Name of module (string) */
#define MODINFO_TYPE		0x0002		/* Type of module (string) */
#define MODINFO_METADATA	0x8000		/* Module-specfic */

#define MODINFOMD_SSYM		0x0003		/* start of symbols */
#define MODINFOMD_ESYM		0x0004		/* end of symbols */

#endif	/* IMAGE_FREEBSD */

#endif /* ELF_H */
