/* Callout/callback interface for Etherboot
 *
 * This file provides the mechanisms for making calls from Etherboot
 * to external programs and vice-versa.
 *
 * Initial version by Michael Brown <mbrown@fensystems.co.uk>, January 2004.
 */

#include "etherboot.h"
#include "callbacks.h"
#include "realmode.h"
#include "segoff.h"
#include <stdarg.h>

/*****************************************************************************
 *
 * IN_CALL INTERFACE
 *
 *****************************************************************************
 */

/* in_call(): entry point for calls in to Etherboot from external code. 
 *
 * Parameters: some set up by assembly code _in_call(), others as
 * passed from external code.
 */
uint32_t i386_in_call ( va_list ap, i386_pm_in_call_data_t pm_data,
			uint32_t opcode ) {
	uint32_t ret;
	i386_rm_in_call_data_t rm_data;
	i386_exit_intercept_t intercept = { NULL };
	in_call_data_t in_call_data = { &pm_data, NULL, &intercept };

	/* Fill out rm_data if we were called from real mode */
	if ( opcode & EB_CALL_FROM_REAL_MODE ) {
		in_call_data.rm = &rm_data;
		rm_data = va_arg ( ap, typeof(rm_data) );
	}
	
	/* Hand off to main in_call() routine */
	ret = in_call ( &in_call_data, opcode, ap );

	/* For some real-mode call types (e.g. MAIN), we can't be sure
	 * that the real-mode routine still exists to return to.  We
	 * therefore provide a method for these calls to intercept the
	 * exit path.
	 */
	if ( intercept.fnc != NULL ) {
		(*(intercept.fnc))( &in_call_data );
		/* Will not reach this point */
	}

	return ret;
}

/* exit_via_prefix(): an exit interceptor that will copy the stored
 * real-mode prefix code back down to base memory and jump to it.
 * This is used as the exit path when we were entered via start16.S
 */
void exit_via_prefix ( in_call_data_t *data ) {
	/* Work out what to do about prefix.  In order of preference:
	 * 1. Return to original location, if prefix code is intact
	 * 2. Execute our copy directly, if possible (i.e. if in base memory)
	 * 3. Copy to the top 512 bytes of free base memory (according
	 *    to BIOS counter at 40:13) and execute there.
	 */

	/* Not yet implemented */
}

/* install_rm_callback_interface(): install real-mode callback
 * interface at specified address.
 *
 * Real-mode code may then call to this address (or lcall to this
 * address plus RM_IN_CALL_FAR) in order to make an in_call() to
 * Etherboot.
 *
 * Returns the size of the installed code, or 0 if the code could not
 * be installed.
 */
int install_rm_callback_interface ( void *address, size_t available ) {
	if ( available &&
	     ( available < rm_callback_interface_size ) ) return 0;

	/* Inform RM code where to find Etherboot */
	rm_etherboot_location = virt_to_phys(_text);

	/* Install callback interface */
	memcpy ( address, &rm_callback_interface,
		 rm_callback_interface_size );

	return rm_callback_interface_size;
}

