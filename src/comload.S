#ifdef	USE_AS86
#define	CON(x)	*x
#define	LOC(x)	x
	.text
#endif
#ifdef	USE_NASM
#define	CON(x)	x
#define	LOC(x)	[x]
	section	.text
#endif

; Cheat a little with the relocations: .COM files are loaded at 0x100
_main:
	push	cs			; generate return address
	mov	ax,CON(retaddr + 0x100)
	push	ax

; Intel didn''t specify an indirect far call that loads the address from
; a near operand - it seems like nobody does computed far calls.  So do it
; the ugly way - dump it on the stack and "return" to the destination.

	mov	ax,cs			; calculate start address of loader
	mov	bx,CON(_body + 0x100)
	mov	cl,CON(4)
	shr	bx,cl
	add	ax,bx
	push	ax			; new code segment
	mov	ax,CON(0x0006)
	push	ax			; new offset

	retf
retaddr:
	int	0x19			; should never reach this


; The body of etherboot is attached here at build time
; Force 16 byte alignment (so that the code segment starts at 0)
#ifdef	USE_AS86
	if	((*-_main)&15) != 0
	rmb	15-((*-_main)&15)
	db	0
	endif
#endif
#ifdef	USE_NASM
	%if	(($-_main)&15) != 0
	times	15-(($-_main)&15) db 0
	db	0
	%endif
#endif
_body:
