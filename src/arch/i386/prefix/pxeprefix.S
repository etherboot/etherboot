/* Offsets of words containing ROM's CS and size (in 512 byte blocks)
 * from start of floppy boot block at 0x7c00
 * Offsets must match those in etherboot.h
 */
#define FLOPPY_SEGMENT	0x7c0
#define ROM_SEGMENT	0x1fa
#define ROM_LENGTH	0x1fc

#define PXENV_UNDI_SHUTDOWN	0x05
#define	PXENV_STOP_UNDI		0x15
#define PXENV_UNLOAD_STACK	0x70
	.text
	.code16
	.org 0
	.globl _start
_start:
	jmp	$FLOPPY_SEGMENT, $n-_start
n:
	movw	%sp, %bp

	push	%cs
	pop	%ds
	
#ifndef PXELOADER_KEEP_ALL
		
	movw	$hellomsgend-hellomsg, %cx
	movw	$hellomsg-_start, %si
	movw	$0x0007, %bx			/* page 0, attribute 7 (normal) */
	movb	$0x0e, %ah			/* write char, tty mode */
prloop_hellomsg: 
	lodsb
	int	$0x10
	loop	prloop_hellomsg
	
	jmp	pxefind
hellomsg:
	.ascii	"PXE loader for etherboot"
	.ascii	"\r\n"
hellomsgend: 
pxe_undi_shutdown_pkt:
	.word	0
pxe_stop_undi_pkt:
	.word   0
pxe_unload_stack_pkt:
	.word	0
	.word	0,0,0,0,0
PXEEntry:
	.word	0,0
pxefind:
	les	4(%bp), %di		/* !PXE structure */
	cmpl	$0x45585021, %es:(%di)	/* '!PXE' signature */
	je	have_pxe
	movl	$0x202b564e, pxemsg2-_start /* 'NV+ ' */
	mov	$0x5650, %ax
	int	$0x1a
	jc	no_pxe
	cmp	$0x564e, %ax
	jne	no_pxe

	cmpl	$0x4e455850, %es:(%bx)	/* 'PXEN' signature */
	jne	no_pxe
	cmpw	$0x2b56, %es:4(%bx)	/* 'V+' signature */
	jne	no_pxe
	mov	%bx, %cs:PXEEntry  -_start /* save them in case we use PXENV+ */
	mov	%es, %cs:PXEEntry+2-_start
	cmpw	$0x201, %es:6(%bx)	/* API version 2.1 or higher */
	jae	get_pxe			/* otherwise use PXENV+ */
use_pxenv:
	push	%bx
	push	%es
	les	%es:0x0a(%bx), %di
	mov	%di, %cs:PXEEntry  -_start
	mov	%es, %cs:PXEEntry+2-_start
	push	%ds
	pop	%es
	mov	$pxe_undi_shutdown_pkt - _start, %di
	mov	$PXENV_UNDI_SHUTDOWN, %bx
	lcall	*%cs:(PXEEntry-_start)
	mov	$pxe_unload_stack_pkt - _start, %di
	mov	$PXENV_UNLOAD_STACK, %bx
	lcall	*%cs:(PXEEntry-_start)
	pop	%es                      /* now adjust avail mem */
	pop	%di
#ifndef PXELOADER_KEEP_UNDI
	mov	%es:0x26(%di), %bx	/* UNDICodeSize */
#endif	
	mov	%es:0x24(%di), %ax      /* UNDICodeSeg */
	cmp	%es:0x20(%di), %ax      /* UNDIDataSeg */
#ifndef PXELOADER_KEEP_UNDI
	jnc	pxe_setmem
#else
	jc	pxe_setmem
#endif
	mov	%es:0x20(%di), %ax	/* UNDIDataSeg */
#ifndef PXELOADER_KEEP_UNDI
	mov	%es:0x22(%di), %bx	/* UNDIDataSize */
#endif
	jmp	pxe_setmem
get_pxe:
	les	%es:0x28(%bx), %di
	cmpl	$0x45585021, %es:(%di)	/* '!PXE' signature */
	jne	no_pxe
	jmp	have_pxe
/*	mov	%cs:PXEEntry+2-_start, %bx restore PXENV+ */
/*	mov	%bx, %es
	mov	%cs:PXEEntry  -_start, %bx
	jmp	use_pxenv */
have_pxe:
	push	%di
	push	%es
	les	%es:0x10(%di), %di
	mov	%di, %cs:PXEEntry  -_start
	mov	%es, %cs:PXEEntry+2-_start

	pushw	%cs
	pushw	$pxe_undi_shutdown_pkt - _start
	pushw	$PXENV_UNDI_SHUTDOWN
	lcall	*%cs:(PXEEntry-_start)
	add	$6, %sp
	pushw	%cs
	pushw	$pxe_stop_undi_pkt
	pushw	$PXENV_STOP_UNDI
	lcall	*%cs:(PXEEntry-_start)
	add	$6,%sp
	pushw   %cs
	pushw	$pxe_unload_stack_pkt - _start
	pushw	$PXENV_UNLOAD_STACK
	lcall	*%cs:(PXEEntry-_start)
	add	$6, %sp

	pop	%es
	pop	%di
#ifndef PXELOADER_KEEP_UNDI
	mov	%es:0x36(%di), %bx	 /* UNDICode.Seg_Size */
#endif
	mov	%es:0x30(%di), %ax	 /* UNDICode.Seg_Addr */
	cmp	%es:0x28(%di), %ax	 /* UNDIData.Seg_Addr */
#ifndef PXELOADER_KEEP_UNDI
	jnc	pxe_setmem
#else
	jc	pxe_setmem
#endif
	mov	%es:0x28(%di), %ax	 /* UNDIData.Seg_Addr */
#ifndef PXELOADER_KEEP_UNDI
	mov	%es:0x2e(%di), %bx	 /* UNDIData.Seg_Size */
#endif
	
pxe_setmem:
#ifndef PXELOADER_KEEP_UNDI
	mov	$4, %cl
	shr	%cl, %bx
	add	%bx, %ax
#endif
	mov	$6, %cl
	shr	%cl, %ax                /* adjust to KB */
	mov	$0x40, %cx
	mov	%cx, %es
	mov	%ax, %es:(0x13)		/* size in memory into (0x413) */

	push	%ds
	pop	%es

	/* print amount of available low memory (<1MB)*/
	/* \begin{hacked assembly} */
	mov	%ax, %bx
	shr	$8, %ax
	add	$0x30, %al
	mov	%al, pxeKmsg-_start+2
	mov	%bl, %al
	shr	$4, %al
	cmp	$10, %al	/* These three lines written by Norbert Juffa <norbert.juffa@amd.com> */
	sbb	$0x69, %al
	das
	mov	%al, pxeKmsg-_start+3
	mov	%bl, %al
	and	$0x0f, %al
	cmp	$10, %al
	sbb	$0x69, %al
	das
	mov	%al, pxeKmsg-_start+4
	movw	$0x0a0d, pxemsg-_start   /* "\r\n" */
	/* \end{hacked assembly} */

	movw	$pxeKmsg-_start, %si
	movw	$pxemsgend-pxeKmsg, %cx
	jmp	pxe_print
#endif /* PXELOADER_KEEP_ALL */
no_pxe:
	movw	$pxemsg-_start, %si
	movw	$pxemsgend-pxemsg, %cx
pxe_print:
	movw	$0x0007, %bx			/* page 0, attribute 7 (normal) */
	movb	$0x0e, %ah			/* write char, tty mode */
prloop_pxemsg: 
	lodsb
	int	$0x10
	loop	prloop_pxemsg

	ljmp	$FLOPPY_SEGMENT + (512/16), $0
pxeKmsg:
	.ascii	"0x123K low memory"
pxemsg:
	.ascii	"No PXE"
pxemsg2:
	.ascii	"    unloaded\r\n"
pxemsgend:
	.org	ROM_SEGMENT
	.word	0
	.word	0
	.word	0xaa55
	.org 512
