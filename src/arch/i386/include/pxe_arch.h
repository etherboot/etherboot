/*
 * Architecture-specific portion of pxe.h for Etherboot
 *
 * See pxe.h for details.
 */

#ifndef PXE_ARCH_H
#define PXE_ARCH_H

/* SEGOFF16_t defined in separate header
 */
#include "segoff.h"
typedef segoff_t I386_SEGOFF16_t;
#define SEGOFF16_t I386_SEGOFF16_t

typedef struct {
	uint16_t		Seg_Addr;
	uint32_t		Phy_Addr;
	uint16_t		Seg_Size;
} PACKED I386_SEGDESC_t;  /* PACKED is required, otherwise gcc pads
			  * this out to 12 bytes -
			  * mbrown@fensystems.co.uk (mcb30) 17/5/03 */
#define SEGDESC_t I386_SEGDESC_t

typedef	uint16_t I386_SEGSEL_t;
#define SEGSEL_t I386_SEGSEL_t

typedef struct {
	uint16_t	opcode;
	segoff_t	segoff;
} PACKED pxe_call_params_t;

#endif /* PXE_ARCH_H */
