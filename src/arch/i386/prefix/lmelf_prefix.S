#include "elf.h"
	.arch sledgehammer
	.code32
	.equ	FLAT_CODE_SEG,_pmcs-_gdt
	.equ	FLAT_DATA_SEG,_pmds-_gdt
	.equ	MSR_K6_EFER,   0xC0000080
	.equ	EFER_LME,      0x00000100
	.equ	X86_CR4_PAE,   0x00000020
	.equ	CR0_PG,        0x80000000

	.section ".hdrs", "a"
	
	/* ELF Header */
	.globl elf_header
elf_header:	
e_ident:	.byte	0x7f, 'E', 'L', 'F', 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0
e_type:		.short	ET_EXEC
e_machine:	.short	EM_X86_64
e_version:	.long	1
e_entry:	.long	_start
e_phoff:	.long	elf_program_header - elf_header
e_shoff:	.long	0
e_flags:	.long	0
e_ehsize:	.short	elf_header_end - elf_header
e_phentsize:	.short	ELF32_PHDR_SIZE
e_phnum:	.short	(elf_program_header_end - elf_program_header)/ELF32_PHDR_SIZE
e_shentsize:	.short	0
e_shnum:	.short	0
e_shstrndx:	.short	0
elf_header_end:
	
elf_program_header:
phdr1_p_type:	.long	PT_NOTE
phdr1_p_offset:	.long	elf_note - elf_header
phdr1_p_vaddr:	.long	elf_note
phdr1_p_paddr:	.long	elf_note
phdr1_p_filesz:	.long	elf_note_end - elf_note
phdr1_p_memsz:	.long	elf_note_end - elf_note
phdr1_p_flags:	.long	PF_R | PF_W | PF_X
phdr1_p_align:	.long	0

/* The decompressor */
phdr2_p_type:	.long	PT_LOAD
phdr2_p_offset:	.long	_start_offset
phdr2_p_vaddr:	.long	_start
phdr2_p_paddr:	.long	_start
phdr2_p_filesz:	.long	_image_size
phdr2_p_memsz:	.long	_image_mem_size
phdr2_p_flags:	.long	PF_R | PF_W | PF_X
phdr2_p_align:	.long	0

elf_program_header_end:

	.globl elf_note
elf_note:
	.balign 4
	.int 2f - 1f
	.int 4f - 3f
	.int EIN_PROGRAM_NAME
1:	.asciz "ELFBoot"
2:
	.balign 4
3:	
	.asciz "Etherboot"
4:


	.balign 4
	.int 2f - 1f
	.int 4f - 3f
	.int EIN_PROGRAM_VERSION
1:	.asciz "ELFBoot"
2:
	.balign 4
3:	
	.asciz VERSION
4:

#if 0
	.balign 4
	.int 2f - 1f
	.int 4f - 3f
	.int EIN_PROGRAM_CHECKSUM
1:	.asciz "ELFBoot"
2:
	.balign 4
3:	
	.word 0
4:
#endif
	.balign 4
elf_note_end:

	.text
	.globl _start
_start:
	.code64
	/* Reload the gdt to something I know */
	leaq	_gdt(%rip), %rax
	movq	%rax, 0x02 + gdtptr(%rip)
	lgdt	gdtptr(%rip)

	/* Enter 32bit compatibility mode */
	leaq	start32(%rip), %rax
	movl	%eax, 0x00 + start32_addr(%rip)
	ljmp	*start32_addr(%rip)

start32:	
	.code32
	/* Reload the data segments */
	movl	$FLAT_DATA_SEG, %eax
	movl	%eax, %ds
	movl	%eax, %es
	movl	%eax, %ss
	
	/* Disable paging */
	movl	%cr0, %eax
	andl	$~CR0_PG, %eax
	movl	%eax, %cr0

	/* Disable long mode */
	movl	$MSR_K6_EFER, %ecx
	rdmsr
	andl	$~EFER_LME, %eax
	wrmsr

	/* Disable PAE */
	movl	%cr4, %eax
	andl	$~X86_CR4_PAE, %eax
	movl	%eax, %cr4

	/* Save the first argument */
	pushl	%ebx
	jmp	_payload_start

	.data
	.globl gdtptr
gdtptr:	
	.word	_gdt_end - _gdt
	.long	_gdt
	.long	0
_gdt:
start32_addr:
	.long	start32
	.long	FLAT_CODE_SEG
_pmcs:
	/* 32 bit protected mode code segment, base 0 */
	.word	0xffff,0
	.byte	0,0x9f,0xcf,0

_pmds:
	/* 32 bit protected mode data segment, base 0 */
	.word	0xffff,0
	.byte	0,0x93,0xcf,0
_gdt_end:	
