#ifdef PCBIOS

#ifdef GAS291
#define DATA32 data32;
#else
#define DATA32 data32
#endif

	.section ".text"
	.section ".text16", "ax", @progbits
	.previous
	.code32

#if defined(CONFIG_TSC_CURRTICKS)
#undef CONFIG_BIOS_CURRTICKS
#else
#define CONFIG_BIOS_CURRTICKS 1
#endif
#if defined(CONFIG_BIOS_CURRTICKS)
/**************************************************************************
CURRTICKS - Get Time
Use direct memory access to BIOS variables, longword 0040:006C (ticks
today) and byte 0040:0070 (midnight crossover flag) instead of calling
timeofday BIOS interrupt.
**************************************************************************/
	.globl	currticks
currticks:
	pushl	%ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi
#ifdef FREEBSD_PXEEMU
	cmpb	$0, pxeemu_nbp_active
	jne	currticks_postswitch
#endif

	pushl	$ 10f
	pushl	$ 20f - 10f
	call	_real_call
	.section ".text16"
10:	.code16
	ret
20:	.code32
	.previous

#ifdef FREEBSD_PXEEMU
currticks_postswitch:
#endif
	movl	0x46C, %eax
	movb	0x470, %bl
	cmpb	$0, %bl
	je	notmidnite
	movb	$0, 0x470		/* clear the flag */
	addl	$0x1800b0,days		/* 0x1800b0 ticks per day */
notmidnite:
	addl	days,%eax
	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret
#endif	/* CONFIG_BIOS_CURRTICKS */

/**************************************************************************
CONSOLE_PUTC - Print a character on console
**************************************************************************/
	.globl	console_putc
console_putc:
#ifdef FREEBSD_PXEEMU
	.globl	pxeemu_console_putc
	cmpb	$0, pxeemu_nbp_active
	je	console_putc_switch

	movb	$0x0e, %ah
	movb	%cl, %al
	pushl	%eax
	call	pxeemu_console_putc
	addl	$4, %esp
	ret
console_putc_switch:
#endif
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	movb	8(%ebp),%cl

	pushl	$ 10f
	pushl	$ 20f - 10f
	call	_real_call
	.section ".text16"
10:	.code16
	movl	$1,%ebx
	movb	$0x0e,%ah
	movb	%cl,%al
	int	$0x10
	ret
20:	.code32
	.previous
#ifdef FREEBSD_PXEEMU
console_putc_exit:
#endif
	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret


/**************************************************************************
CONSOLE_GETC - Get a character from console
**************************************************************************/
	.globl	console_getc
console_getc:
	pushl	%ebx
	pushl	%esi
	pushl	%edi

	pushl	$ 10f
	pushl	$ 20f - 10f
	call	_real_call
	.section ".text16"
10:	.code16
	
	movb	$0x0,%ah
	int	$0x16
	movb	%al,%bl
	ret
20:	.code32
	.previous
	xor	%eax,%eax
	movzbl	%bl,%eax

	popl	%edi
	popl	%esi
	popl	%ebx
	ret

/**************************************************************************
CONSOLE_ISCHAR - Check for keyboard interrupt
**************************************************************************/
	.globl	console_ischar
console_ischar:
	pushl	%ebx
	pushl	%esi
	pushl	%edi

	pushl	$ 10f
	pushl	$ 20f - 10f
	call	_real_call
	.section ".text16"
10:	.code16
	
	xorw	%bx,%bx
	movb	$0x1,%ah
	int	$0x16
	jz	2f
	movb	%al,%bl
2:
	ret
20:	.code32
	.previous
	movzbl	%bl,%eax
	popl	%edi
	popl	%esi
	popl	%ebx
	ret

/**************************************************************************
GETSHIFT - Get keyboard shift state
**************************************************************************/
	.globl	getshift
getshift:
	pushl	%ebx
	pushl	%esi
	pushl	%edi

	pushl	$ 10f
	pushl	$ 20f - 10f
	.section ".text16"
10:	.code16
	
	movb	$2,%ah
	int	$0x16
	andb	$0xdf,%al
	movw	%ax,%bx
	ret
20:	.code32
	.previous
	movzbl	%bl,%eax
	popl	%edi
	popl	%esi
	popl	%ebx
	ret

/**************************************************************************
INT15 - Call Interrupt 0x15
**************************************************************************/
	.globl	int15
int15:
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	movl	8(%ebp),%ecx	/* _real_call destroys %eax */

	pushl	$ 10f
	pushl	$ 20f - 10f
	call	_real_call
	.section ".text16"
10:	.code16
	movw	%cx,%ax
	stc			/* in case function not supported */
	int	$0x15
	jc	int15status
	movb	$0,%ah
int15status:
	movb	%ah,%bl		/* _real_call destroys %eax */
	ret
20:	.code32
	.previous
	movzbl	%bl,%eax
	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret

#ifdef	POWERSAVE
/**************************************************************************
CPU_NAP - Save power by halting the CPU until the next interrupt
**************************************************************************/
	.globl	cpu_nap
cpu_nap:
	pushl	%ebx
	pushl	%esi
	pushl	%edi

	pushl	$ 10f
	pushl	$ 20f - 10f
	call	_real_call
	.section ".text16"
10:	.code16
	hlt
	ret
20:	.code32
	.previous
	popl	%edi
	popl	%esi
	popl	%ebx
	ret
#endif	/* POWERSAVE */


/**************************************************************************
E820_MEMSIZE - Get a listing of memory regions
**************************************************************************/
	.globl meme820
#define SMAP	0x534d4150
meme820:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi

	movl	8(%ebp), %edi	/* Address to return e820 structures at */
	movl	12(%ebp), %esi	/* Maximum number of e820 structurs to return */
	pushl	%esi

	/* Multiply %esi by 20 */
	movl	%esi, %eax
	movl	%esi, %ebx
	shll	$2, %eax
	shll	$4, %ebx
	addl	%eax, %ebx
	pushl	%ebx
	
	pushl	$ 10f
	pushl	$ 20f - 10f
	call	_real_call
	.section ".text16"
10:	.code16
	/* Allocate space on the stack for the maximum # of e820 structures */
	popw	%ax
	subw	%bx, %sp
	movw	%sp, %di
	pushw	%ax

jmpe820:
	movl	$0xe820, %eax
	movl	$SMAP, %edx
	movl	$20, %ecx
	/* %di was setup earlier */
	int	$0x15
	jc	bail820

	cmpl	$SMAP, %eax
	jne	bail820

good820:	
	/* If this is useable memory, we save it by simply advancing %di by
	 * sizeof(e820rec)
	 */
	decl	%esi
	testl	%esi,%esi
	jz	bail820

	addw	$20, %di
again820:
	cmpl	$0, %ebx	/* check to see if %ebx is set to EOF */
	jne	jmpe820

bail820:
	ret
20:	.code32
	.previous
	popl	%ecx		/* Get the maximum return size */
	pushl	%esi		/* Save the structure count */

	/* Copy the structures to the return buffer */
	movl	%eax, %esi
	movl	8(%ebp), %edi
	rep	movsb

	popl	%esi
	popl	%eax
	subl	%esi, %eax	/* Compute how many structure we read */

	/* Restore everything else */	
	popl	%edi
	popl	%esi
	popl	%ebx
	movl	%ebp, %esp
	popl	%ebp
	ret

	
/**************************************************************************
MEMSIZE - Determine size of extended memory
**************************************************************************/
	.globl	memsize
memsize:
	pushl	%ebx
	pushl	%esi
	pushl	%edi

	pushl	$ 10f
	pushl	$ 20f - 10f
	call	_real_call
	.section ".text16"
10:	.code16
	stc					# fix to work around buggy
	xorw	%cx,%cx				# BIOSes which dont clear/set
	xorw	%dx,%dx				# carry on pass/error of
						# e801h memory size call
						# or merely pass cx,dx though
						# without changing them.
	movw	$0xe801,%ax
	int	$0x15
	jc	3f
	cmpw	$0,%cx				# Kludge to handle BIOSes
	jne	1f				# which report their extended
	cmpw	$0,%dx				# memory in AX/BX rather than
	je	2f				# CX/DX.  The spec I have read
1:
	movw	%cx,%ax				# seems to indicate AX/BX 
	movw	%dx,%bx				# are more reasonable anyway...
2:
	andl	$0xffff,%eax
	andl	$0xffff,%ebx
	shll	$6,%ebx
	addl	%ebx,%eax
	jmp	4f
3:
	movw	$0x8800,%ax
	int	$0x15
	andl	$0xffff,%eax
4:
	movl	%eax,%esi
	ret
20:	.code32
	.previous
	movl	%esi,%eax
	popl	%edi
	popl	%esi
	popl	%ebx
	ret

/**************************************************************************
BASEMEMSIZE - Get size of the conventional (base) memory
**************************************************************************/
	.globl	basememsize
basememsize:
	pushl	$ 10f
	pushl	$ 20f - 10f
	call	_real_call
	.section ".text16"
10:	.code16
	int	$0x12
	movw	%ax,%cx
	ret
20:	.code32
	.previous
	movw	%cx,%ax
	ret

/**************************************************************************
DISK_INIT - Initialize the disk system
**************************************************************************/
#ifdef	CAN_BOOT_DISK
	.globl	disk_init
disk_init:
	pushl	%ebx
	pushl	%esi
	pushl	%edi

	pushl	$ 10f
	pushl	$ 20f - 10f
	call	_real_call
	.section ".text16"
10:	.code16
	xorw	%ax,%ax
	movb	$0x80,%dl
	int	$0x13
	ret
20:	.code32
	.previous
	popl	%edi
	popl	%esi
	popl	%ebx
	ret
#endif

/**************************************************************************
DISK_READ - Read a sector from disk
**************************************************************************/
#ifdef	CAN_BOOT_DISK
	.globl	disk_read
disk_read:
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	movb	8(%ebp),%dl	/* drive number */
	movb	16(%ebp),%dh	/* head number */
	movb	12(%ebp),%ch	/* cylinder number */
	movb	13(%ebp),%cl	/* cylinder number */
	shl	$6,%cl
	orb	20(%ebp),%cl	/* sector number */
	movw	26(%ebp),%si
	rorw	$4,%si
	movw	24(%ebp),%bx	/* buffer */

	pushl	$ 10f
	pushl	$ 20f - 10f
	call	_real_call
	.section ".text16"
10:	.code16
	movw	$0x0201,%ax
	movw	%si,%es
	int	$0x13
	jc	1f
	xorw	%ax,%ax
1:
	movw	%ax,%bx
	ret
20:	.code32
	.previous
	movzwl	%bx,%eax
	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret
#endif

#ifdef FREEBSD_PXEEMU
.globl initsp
.globl pxeemu_nbp_active
pxeemu_nbp_active: 	.byte	0
#endif

days:	.long	0

#endif /* PCBIOS */
