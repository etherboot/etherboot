/* Header for pxe_callbacks.c.
 */

#ifndef PXE_CALLBACKS_H
#define PXE_CALLBACKS_H

#include "etherboot.h"
#include "segoff.h"
#include "pxe.h"

typedef struct {
	segoff_t	orig_retaddr;
	uint16_t	opcode;
	segoff_t	segoff;
} PACKED pxe_call_params_t;

typedef struct {
	pxe_t		pxe	__attribute__ ((aligned(16)));
	pxenv_t		pxenv	__attribute__ ((aligned(16)));
} pxe_stack_t;

/*
 * These values are hard-coded into the PXE spec
 */
#define PXE_LOAD_SEGMENT	(0x0000)
#define PXE_LOAD_OFFSET		(0x7c00)
#define PXE_LOAD_ADDRESS	( ( PXE_LOAD_SEGMENT << 4 ) + PXE_LOAD_OFFSET )

/* Function prototypes
 */
pxe_stack_t * install_pxe_stack ( void *base );
void remove_pxe_stack ( pxe_stack_t *pxe_stack );
extern int xstartpxe ( pxe_stack_t *pxe_stack );

#endif /* PXE_CALLBACKS_H */