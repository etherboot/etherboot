/* Callout/callback interface for Etherboot
 *
 * This file provides the mechanisms for making calls from Etherboot
 * to external programs and vice-versa.
 *
 * Initial version by Michael Brown <mbrown@fensystems.co.uk>, January 2004.
 *
 * $Id$
 */

#ifndef CALLBACKS_H
#define CALLBACKS_H

/* Opcodes and flags for in_call()
 */
#define EB_OPCODE(x) ( (x) & 0xffff )
#define EB_OPCODE_MAIN		(0x0000)
#define EB_OPCODE_CHECK		(0x6948)	/* 'Hi' */
#define EB_OPCODE_PXE		(0x7850)	/* 'Px' */
#define EB_OPCODE_PXENV		(0x7650)	/* 'Pv' */
#define EB_USE_INTERNAL_STACK	( 1 << 16 )
#define EB_CALL_FROM_REAL_MODE	( 1 << 17 )	/* i386 only */

/* Standard return codes
 */
#define EB_CHECK_RESULT		(0x6f486948)	/* 'HiHo' */

/* Types of data that can be passed to external_call().
 *
 * Several of these are architecture-specific, but the easiest way to
 * ensure uniqueness is to put them all here.
 */
#define EXTCALL_NONE		(0x0000)
#define EXTCALL_REGISTERS	(0x0001)
#define EXTCALL_SEG_REGISTERS	(0x0002)
#define EXTCALL_STACK		(0x0004)
#define EXTCALL_RET_STACK	(0x0005)
#define EXTCALL_RELOC_STACK	(0x0006)
#define EXTCALL_TRAMPOLINE	(0x0007)
#define EXTCALL_VA_LIST		(0x0008)
#define EXTCALL_END_LIST	(0xffff)

/* Include arch-specific callbacks bits
 */
#include "callbacks_arch.h"

/* Skip the definitions that won't make sense to the assembler */
#ifndef ASSEMBLY

#include <stdarg.h>

#define EP_STACK(structure) \
	EXTCALL_STACK, (structure), sizeof(*(structure))

#define EP_RET_STACK(structure) \
	EXTCALL_RET_STACK, (structure), sizeof(*(structure)), NULL
#define EP_RET_VARSTACK(structure,length) \
	EXTCALL_RET_STACK, (structure), sizeof(*(structure)), (length)

#define EP_RELOC_STACK(top) EXTCALL_RELOC_STACK, (top)

#define EP_TRAMPOLINE(start,end) \
	EXTCALL_TRAMPOLINE, (void*)(start), ((void*)(end) - (void*)(start))
#define EC_TRAMPOLINE_CALL ( NULL )

#define EP_VA_LIST(ap) EXTCALL_VA_LIST, (ap), 0

#define EP_TRACE EXTCALL_NONE

/* Function prototypes */
extern int _ext_call ( void *address, ... );
extern int _v_ext_call ( void *address, va_list ap );
#define ext_call(address, ...) \
	_ext_call( (void*)(address), ##__VA_ARGS__, EXTCALL_END_LIST )

#ifdef DEBUG_EXT_CALL
extern int v_ext_call ( void *address, va_list ap );
extern int v_ec_check_param ( int type, va_list *ap );
extern int v_arch_ec_check_param ( int type, va_list *ap );
extern int v_ext_call_check ( void *address, va_list ap );
#define ext_call_trace(address, ...) \
	ext_call( (address), EP_TRACE, ##__VA_ARGS__ )
#else
#define v_ext_call(address, ...) _v_ext_call( (address), ##__VA_ARGS__ )
#define ext_call_trace(address, ...) ext_call( (address), ##__VA_ARGS__ )
#endif

#ifndef in_call_data_t
typedef struct {} empty_struct_t;
#define in_call_data_t empty_struct_t
#endif

#endif /* ASSEMBLY */

#endif /* CALLBACKS_H */
