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
#include "pxe_export.h"

int pxe_api_call ( int opcode, t_PXENV_ANY *params ) {
	int i;

	printf ( "[PXE-call %hx]", opcode );
	/*	for ( i = 0; i < 16; i++ ) {
		printf ( "%hhx ", ((char*)params)[i] );
		}
		putchar ( '\n' ); */
	params->Status = PXENV_STATUS_FAILURE;
	return PXENV_EXIT_FAILURE;
}

#endif /* PXE_EXPORT */
