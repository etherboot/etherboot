/* At entry, the processor is in 16 bit real mode and the code is being
 * executed from an address it was not linked to. Code must be pic and
 * 32 bit sensitive until things are fixed up.
 */


/* LZHuf (LZSS) Decompressing boot loader for ROM images
 *
 * this code is based on the work of Haruyasu Yoshizaki and Haruhiko Okumura
 * who implemented the original compressor and decompressor in C code
 *
 * Converted to 32bit assembly 16 July 2002 Eric Biederman <ebiederman@lnxi.com>
 *
 * Copyright 1997 by M. Gutschke <gutschk@math.uni-muenster.de>
 *
 * Compression pays off, as soon as the uncompressed image is bigger than
 * about 1.5kB. This assumes an average compressibility of about 60%.
 */


/* Do not change these values unless you really know what you are doing
 * the pre-computed lookup tables rely on the buffer size being 4kB or
 * smaller. The buffer size must be a power of two. The lookahead size has
 * to fit into 6 bits. If you change any of these numbers, you will also
 * have to adjust the compressor accordingly.
 */
#define BUFSZ		4096
#define LOOKAHEAD	60
#define THRESHOLD	2
#define NCHAR		(256+LOOKAHEAD-THRESHOLD)
#define TABLESZ		(NCHAR+NCHAR-1)
#define ROOT		(TABLESZ-1)

.text
#if 1

#define EIN_PROGRAM_NAME	0x00000001
/* The program in this ELF file */
#define EIN_PROGRAM_VERSION	0x00000002
/* The version of the program in this ELF file */
#define EIN_PROGRAM_CHECKSUM	0x00000003
/* ip style checksum of the memory image. */

#define ET_EXEC 2
#define EM_386	3
#define ELF32_PHDR_SIZE (8*4)
#define PT_NULL 0
#define PT_LOAD 1
#define PT_NOTE 4
#define PF_X	(1<< 0)
#define PF_W	(1<< 1)
#define PF_R	(1<< 2)

		/* ELF Header */
		.globl elf_header
elf_header:	
e_ident:	.byte	0x7f, 'E', 'L', 'F', 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0
e_type:		.short	ET_EXEC
e_machine:	.short	EM_386
e_version:	.long	1
e_entry:	.long	init
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
phdr2_p_offset:	.long	init - elf_header
phdr2_p_vaddr:	.long	init
phdr2_p_paddr:	.long	init
phdr2_p_filesz:	.long	_edata - init
phdr2_p_memsz:	.long	_end - init
phdr2_p_flags:	.long	PF_R | PF_W | PF_X
phdr2_p_align:	.long	0

/* Claim space for the decompressed code */
phdr3_p_type:	.long	PT_LOAD
phdr3_p_offset:	.long	0
phdr3_p_vaddr:	.long	spaces
phdr3_p_paddr:	.long	spaces
phdr3_p_filesz:	.long	0
phdr3_p_memsz:	.long	0x10000 - 3072 - (spaces & 0xFFFF)
phdr3_p_flags:	.long	PF_R | PF_W | PF_X
phdr3_p_align:	.long	0

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
elf_note_end:

#endif
start:
	cli
/*
 * Save the initial register contents
 */
	movl	%eax, initial_regs+0
	movl	%ebx, initial_regs+4
	movl	%ecx, initial_regs+8
	movl	%edx, initial_regs+12
	movl	%esi, initial_regs+16
	movl	%edi, initial_regs+20
	movl	%esp, initial_regs+24
	movl	%ebp, initial_regs+28

	movl	$_estack, %esp
	
/*
 * INIT -- initializes all data structures
 * ====
 */

init:	
	cld
	movl	$dcodrle, %esi		/* uncompress run length encoded */
	movl	$dcode, %edi		/* lookup table for codes */
	movb	$6, %dl
	movb	$0x20, %dh
	xorb	%bh,%bh
init0:	
	lodsb
	movb	%al,%bl
init1:
	xorl	%ecx, %ecx
	movb	%dh,%cl
	movb	%bh,%al
	rep
	stosb
	incb	%bh
	decb	%bl
	jnz	init1
	shrb	%dh
	decb	%dl
	jnz	init0
	movb	$1, %bl			/* uncompress run length encoded */
	movb	$6, %bh			/* lookup table for code lengths */
init2:	
	lodsb
	xorl	%ecx, %ecx
	movb	%al,%cl
	movb	%bl,%al
	rep
	stosb
	incb	%bl
	decb	%bh
	jnz	init2

	movl	$NCHAR, %ecx		/* set all frequencies of leaf nodes */
	movw	$1, %ax			/* to one */
	rep
	stosw
	movl	$freq, %esi
	movl	$ROOT+1-NCHAR, %ecx
init3:	
	lodsw				/* update frequencies of non-leaf nodes */
	movw	%ax,%bx
	lodsw
	addw	%bx,%ax
	stosw
	loop	init3
	movw	$0xFFFF, %ax
	stosw				/* sentinel with infinite frequency */
	movl	$NCHAR, %ecx
	movw	$TABLESZ, %ax
init4:	
	stosw				/* update son pointers for leaf nodes */
	incw	%ax
	loop	init4
	movl	$ROOT+1-NCHAR, %ecx
	xorw	%ax,%ax
init5:	
	stosw				/* update son ptrs for non-leaf nodes */
	addw	$2, %ax
	loop	init5
	movl	$ROOT+1-NCHAR, %ecx
	movw	$NCHAR, %ax
init6:	
	stosw				/* update parent ptrs for non-leaf nd. */
	stosw
	incw	%ax
	loop	init6
	movl	$NCHAR, %ecx
	xorw	%ax,%ax
	stosw				/* root node has no parent */
init7:	
	stosw				/* update parent ptrs for leaf nodes */
	incw	%ax
	loop	init7
	xorw	%ax,%ax
	stosb				/* clear getlen */
	stosw				/* clear getbuf */
	movb	$0x20, %al		/* fill text buffer with spaces */
	movl	$spaces, %edi
	movl	$BUFSZ-LOOKAHEAD, %ecx
	rep
	
	stosb
	/* fall thru */


/* 
 * MAIN -- reads compressed codes and writes decompressed data
 * ====
 */

	movl	$_payload, %esi		/* get length of compressed data stream */
	movl	$uncompressed, %edi

	lodsl
	movl	%eax, %ecx
main1:	
	pushl	%ecx
	call	dcdchr			/* decode one code symbol */
	orb	%ah,%ah			/* test if 8bit character */
	jnz	main2
	stosb				/* store verbatim */
	popl	%ecx
	loop	main1			/* proceed with next compressed code */
	jmp	done			/* until end of input is detected */
main2:	
	pushl	%eax
	call	dcdpos			/* compute position in output buffer */
	movl	%esi, %eax
	subl	%edi, %ebx
	notl	%ebx
	movl	%ebx, %esi		/* si := di - dcdpos() - 1 */
	popl	%ecx
	subl	$255-THRESHOLD, %ecx	/* compute length of code sequence */
	movl	%ecx, %edx
	rep
	movsb
	movl	%eax,%esi
	popl	%ecx
	subl	%edx, %ecx		/* check end of input condition */
	jnz	main1			/* proceed with next compressed code */
done:
	/* Start Etherboot */
	movl	initial_regs+0,  %eax
	movl	initial_regs+4,  %ebx
	movl	initial_regs+8,  %ecx
	movl	initial_regs+12, %edx
	movl	initial_regs+16, %esi
	movl	initial_regs+20, %edi
	movl	initial_regs+24, %esp
	movl	initial_regs+28, %ebp
	jmp	RELOC
/*
 * GETBIT -- gets one bit pointed to by DS:ESI
 * ======
 *
 * changes: AX,CX,DL
 */

getbit:
	movb	$8, %cl
	movb	getlen, %dl		/* compute number of bits required */
	subb	%dl,%cl			/* to fill read buffer */
	jae	getbit1
	movw	getbuf, %ax		/* there is still enough read ahead data */
	jmp	getbit2
getbit1:
	lodsb				/* get next byte from input stream */
	xorb	%ah,%ah
	shlw	%cl,%ax			/* shift, so that it will fit into */
	movw	getbuf, %cx		/* read ahead buffer */
	orw	%cx,%ax
	addb	$8, %dl			/* update number of bits in buffer */
getbit2:
	movw	%ax,%cx
	shlw	%cx			/* extract one bit from buffer */
	movw	%cx, getbuf
	decb	%dl
	movb	%dl, getlen		/* and update number of bits */
	shlw	%ax			/* return in carry flag */
	ret


/*
 * DCDPOS -- decodes position in textbuffer as pointed to by DS:SI, result in BX
 * ======
 *
 * changes: AX,EBX,ECX,DX
 */

dcdpos:
	movl	$0x0800, %ebx
dcdpos1:
	shlb	%bl			/* read one byte */
	call	getbit
	jnc	dcdpos2
	incb	%bl
dcdpos2:
	decb	%bh
	jnz	dcdpos1
	movb	%bl,%dh			/* read length of code from table */
	xorb	%bh,%bh
	xorl	%ecx, %ecx
	movb	dlen(%ebx),%cl
	movb	dcode(%ebx),%bl		/* get top six bits from table */
	shll	$6, %ebx
dcdpos3:
	pushl	%ecx			/* read the rest from the input stream */
	shlb	%dh
	call	getbit
	jnc	dcdpos4
	incb	%dh
dcdpos4:
	popl	%ecx
	loop	dcdpos3
	andb	$0x3f, %dh		/* combine upper and lower half of code */
	orb	%dh,%bl
	ret

/*
 * DCDCHR -- decodes one compressed character pointed to by DS:SI
 * ======
 *
 * changes: AX,BX,CX,DX
 */

dcdchr:
	movl	$ROOT, %ebx		/* start at root entry */
	shll	%ebx
	movzwl	son(%ebx),%ebx
dcdchr1:
	call	getbit			/* get a single bit */
	jnc	dcdchr2
	incl	%ebx			/* travel left or right */
dcdchr2:
	shll	%ebx
	movzwl	son(%ebx), %ebx
	cmpl	$TABLESZ, %ebx		/* until we come to a leaf node */
	jb	dcdchr1
	movl	%ebx, %eax
	subl	$TABLESZ, %eax
	/* fall thru */

/*
 * UPDATE -- updates huffman tree after incrementing frequency for code in BX
 * ======
 *
 * changes: BX,CX,DX
 */

update:
	/* we do not check whether the frequency count has overrun.
	 * this will cause problems for large files, but be should be fine
	 * as long as the compressed size does not exceed 32kB and we
	 * cannot do more than this anyways, because we load into the
	 * upper 32kB of conventional memory
	 */
	pushl	%esi
	pushl	%eax
	shll	%ebx
	movzwl	parent(%ebx),%ebx
update1:
	shll	%ebx
	movzwl	freq(%ebx), %edx
	incl	%edx			/* increment frequency count by one */
	movw	%dx, freq(%ebx)
	movl	%ebx, %esi
	movl	%ebx, %esi
	addl	$freq+2, %esi
	lodsw				/* check if nodes need reordering */
	cmpw	%ax, %dx
	jbe	update5
update2:
	lodsw
	cmpw	%dx, %ax
	jb	update2
	movzwl	-4(%esi), %ecx
	movw	%cx, freq(%ebx)		/* swap frequency of entries */
	movw	%dx, -4(%esi)
	movl	%esi, %eax		/* compute index of new entry */
	subl	$freq+4, %eax
	movl	%eax, %edx
	shrl	%eax
	movzwl	son(%ebx), %ecx		/* get son of old entry */
	movl	%ecx, %esi
	addl	%esi, %esi
	movw	%ax, parent(%esi)	/* and update the ptr to new parent */
	cmpl	$TABLESZ, %ecx
	jae	update3			/* do this for both branches */
	movw	%ax, parent+2(%esi)	/* if not a leaf node */
update3:
	movl	%edx, %esi
	movzwl	son(%esi), %edx		/* get son of new entry */
	movw	%cx, son(%esi)		/* update its contents */
	movl	%edx, %esi
	addl	%esi, %esi
	movl	%ebx, %ecx
	shrl	%ecx
	movw	%cx, parent(%esi)	/* and update the ptr to new paren */
	cmpl	$TABLESZ, %edx
	jae	update4			/* do this for both branches */
	movw	%cx, parent+2(%esi)	/* if not a leaf node */
update4:
	movw	%dx, son(%ebx)		/* update son of old entry */
	movl	%eax, %ebx		/* continue with new entry */
	shll	%ebx
update5:
	movzwl	parent(%ebx), %ebx	/* continue with parent */
	orl	%ebx, %ebx
	jnz	update1			/* until we found the root entry */
	popl	%eax
	popl	%esi
	ret

/*
 * constant data. this part of the program resides in ROM and cannot be
 * changed
 *
 * run length encoded tables will be uncompressed into the bss segment
 * take care with any symbols here for .com files to add 0x100 to address
 */

dcodrle: .byte 0x01,0x03,0x08,0x0C,0x18,0x10
dlenrle: .byte 0x20,0x30,0x40,0x30,0x30,0x10

/*
 * variable data segment (bss)
 * this segment will always be found at 0x90000 (i.e. at RELOC - SCRATCH)
 *
 * do not change the order or the sizes of any of the following tables
 * the initialization code makes assumptions on the exact layout of the
 * data structures...
 */

.bss
/* lookup table for index into buffer of recently output characters */
dcode:	.skip	256
	
/* lookup table for length of code sequence from buffer of recent characters */
dlen:	.skip	256
	
/* table with frequency counts for all codes */
freq:	.skip	2*(TABLESZ+1)
	
/* pointer to child nodes */
son:	.skip	2*(TABLESZ)
	
/* the first part of this table contains all the codes	(0..TABLESZ-1) */
/* the second part contains all leaf nodes		(TABLESZ..) */
parent:	.skip	2*(TABLESZ+NCHAR)
	
/* temporary storage for extracting bits from compressed data stream */
getlen:	.skip	1
getbuf:	.skip	1

	.balign 4
initial_regs:
	.skip	8*4

	.p2align 3
	/* Allocate a 4K stack in the bss segment */
_stack:
	.skip 4096
_estack:
			
	/* uncompressed data will be written to RELOC */
.equ	uncompressed, RELOC

/* the initial buffer has to be filled with spaces (size: BUFSZ+LOOKAHEAD) */
.equ	spaces, uncompressed-BUFSZ+LOOKAHEAD

