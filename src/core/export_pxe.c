/* PXE API interface for Etherboot.
 *
 * Copyright (C) 2004 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifdef PXE_EXPORT

#include "etherboot.h"
#include "pxe.h"
#include "callbacks.h"
#include <stdarg.h>

int pxe_in_call ( va_list params, in_call_data_t *in_call_data ) {

#ifdef __i386__
	/* i386 calling conventions; the only two defined by Intel's
	 * PXE spec.
	 *
	 * Assembly code must pass a long containing the PXE version
	 * code (i.e. 0x201 for !PXE, 0x200 for PXENV+) as the first
	 * parameter after the in_call opcode.  This is used to decide
	 * whether to take parameters from the stack (!PXE) or from
	 * registers (PXENV+).
	 */
	uint32_t api_version = va_arg ( params, typeof(api_version) );
	uint16_t opcode;
	segoff_t segoff;
	void *structure;
		
	if ( api_version >= 0x201 ) {
		/* !PXE calling convention */
		pxe_call_params_t pxe_params
			= va_arg ( params, typeof(pxe_params) );
		opcode = pxe_params.opcode;
		segoff = pxe_params.segoff;
	} else {
		/* PXENV+ calling convention */
		opcode = in_call_data->pm->regs.bx;
		segoff.segment = in_call_data->rm->seg_regs.es;
		segoff.offset = in_call_data->pm->regs.di;
	}
	structure = VIRTUAL ( segoff.segment, segoff.offset );
	return pxe_api_call ( opcode, structure );
#else
#error "This architecture does not have a defined PXE API calling convention"
#endif

}

int pxe_api_call ( int opcode, void *structure ) {
	int i;

	printf ( "PXE API call %hx\nParameters: ", opcode );
	for ( i = 0; i < 16; i++ ) {
		printf ( "%hhx ", ((char*)structure)[i] );
	}
	putchar ( '\n' );
	return 7;
}

#endif /* PXE_EXPORT */
