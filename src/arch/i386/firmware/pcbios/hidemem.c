/* Utility functions to hide Etherboot by manipulating the E820 memory
 * map.  These could go in memsizes.c, but are placed here because not
 * all images will need them.
 */

#include "etherboot.h"
#include "hidemem.h"

static int mangling = 0;
static void *mangler = NULL;

#define INSTALLED(x)	( (typeof(&x)) ( (void*)(&x) - (void*)e820mangler \
					 + mangler ) )
#define intercept_int15		INSTALLED(_intercept_int15)
#define intercepted_int15	INSTALLED(_intercepted_int15)
#define hide_memory		INSTALLED(_hide_memory)
#define INT15_VECTOR ( (segoff_t*) ( phys_to_virt( 4 * 0x15 ) ) )

int install_e820mangler ( void *new_mangler ) {
	if ( mangling ) return 0;
	memcpy ( new_mangler, &e820mangler, e820mangler_size );
	mangler = new_mangler;
	return 1;
}

/* Intercept INT15 calls and pass them through the mangler.  The
 * mangler must have been copied to base memory before making this
 * call, and "mangler" must point to the base memory copy, which must
 * be 16-byte aligned.
 */
int hide_etherboot ( void ) {
	if ( mangling ) return 1;
	if ( !mangler ) return 0;

	/* Hook INT15 handler */
	*intercepted_int15 = *INT15_VECTOR;
	(*hide_memory)[0].start = virt_to_phys(_text);
	(*hide_memory)[0].length = _end - _text;
	/* IMPORTANT, possibly even FIXME:
	 *
	 * Etherboot has a tendency to claim a very large area of
	 * memory as possible heap; enough to make it impossible to
	 * load an OS if we hide all of it.  We hide only the portion
	 * that's currently in use.  This means that we MUST NOT
	 * perform further allocations from the heap while the mangler
	 * is active.
	 */
	(*hide_memory)[1].start = heap_ptr;
	(*hide_memory)[1].length = heap_bot - heap_ptr;
	INT15_VECTOR->segment = SEGMENT(mangler);
	INT15_VECTOR->offset = 0;

	mangling = 1;
	return 1;
}

int unhide_etherboot ( void ) {
	if ( !mangling ) return 1;

	/* Restore original INT15 handler
	 *
	 * FIXME: should probably check that is safe to do so
	 * (i.e. that no-one else has hooked it), but what do we do if
	 * it isn't?  We can't leave our handler hooked in.
	 */
	*INT15_VECTOR = *intercepted_int15;

	mangling = 0;
	return 1;
}
