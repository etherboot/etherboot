/*
 * Basic support for controlling the 8259 Programmable Interrupt Controllers.
 *
 * Initially written by Michael Brown (mcb30).
 */

#include <etherboot.h>
#include <pic8259.h>

#ifdef DEBUG_IRQ
#define IRQ_DBG(...) printf ( __VA_ARGS__ )
#else
#define IRQ_DBG(...)
#endif

/* Current locations of trivial IRQ handler.  These will change at
 * runtime when relocation is used; the handler needs to be copied to
 * base memory before being installed.
 */
void (*trivial_irq_handler)P((void)) = _trivial_irq_handler;
uint16_t volatile *trivial_irq_triggered = &_trivial_irq_triggered;
segoff_t *trivial_irq_chain_to = &_trivial_irq_chain_to;
uint8_t *trivial_irq_chain = &_trivial_irq_chain;
irq_t trivial_irq_installed_on = IRQ_NONE;

/* Install a handler for the specified IRQ.  Address of previous
 * handler will be stored in previous_handler.  Enabled/disabled state
 * of IRQ will be preserved across call, therefore if the handler does
 * chaining, ensure that either (a) IRQ is disabled before call, or
 * (b) previous_handler points directly to the place that the handler
 * picks up its chain-to address.
 */

int install_irq_handler ( irq_t irq, segoff_t *handler,
			  uint8_t *previously_enabled,
			  segoff_t *previous_handler ) {
	segoff_t *irq_vector = IRQ_VECTOR ( irq );
	*previously_enabled = irq_enabled ( irq );

	if ( irq > IRQ_MAX ) {
		IRQ_DBG ( "Invalid IRQ number %d\n" );
		return 0;
	}

	previous_handler->segment = irq_vector->segment;
	previous_handler->offset = irq_vector->offset;
	if ( *previously_enabled ) disable_irq ( irq );
	IRQ_DBG ( "Installing handler at %hx:%hx for IRQ %d, leaving %s\n",
		  handler->segment, handler->offset, irq,
		  ( *previously_enabled ? "enabled" : "disabled" ) );
	IRQ_DBG ( "...(previous handler at %hx:%hx)\n",
		  previous_handler->segment, previous_handler->offset );
	irq_vector->segment = handler->segment;
	irq_vector->offset = handler->offset;
	if ( *previously_enabled ) enable_irq ( irq );
	return 1;
}

/* Remove handler for the specified IRQ.  Routine checks that another
 * handler has not been installed that chains to handler before
 * uninstalling handler.  Enabled/disabled state of the IRQ will be
 * restored to that specified by previously_enabled.
 */

int remove_irq_handler ( irq_t irq, segoff_t *handler,
			 uint8_t *previously_enabled,
			 segoff_t *previous_handler ) {
	segoff_t *irq_vector = IRQ_VECTOR ( irq );

	if ( irq > IRQ_MAX ) {
		IRQ_DBG ( "Invalid IRQ number %d\n" );
		return 0;
	}
	if ( ( irq_vector->segment != handler->segment ) ||
	     ( irq_vector->offset != handler->offset ) ) {
		IRQ_DBG ( "Cannot remove handler for IRQ %d\n" );
		return 0;
	}

	disable_irq ( irq );
	irq_vector->segment = previous_handler->segment;
	irq_vector->offset = previous_handler->offset;
	if ( *previously_enabled ) enable_irq ( irq );
	return 1;
}

int install_trivial_irq_handler ( irq_t irq ) {
	int installed = 0;
	segoff_t trivial_irq_handler_segoff = SEGOFF(trivial_irq_handler);
	
	if ( trivial_irq_installed_on != IRQ_NONE ) {
		IRQ_DBG ( "Can install trivial IRQ handler only once\n" );
		return 0;
	}

	IRQ_DBG ( "Installing trivial IRQ handler on IRQ %d\n", irq );
	installed = install_irq_handler ( irq, &trivial_irq_handler_segoff,
					  trivial_irq_chain,
					  trivial_irq_chain_to );
	if ( ! installed ) return 0;
	trivial_irq_installed_on = irq;

	IRQ_DBG ( "Testing trivial IRQ handler\n" );
	disable_irq ( irq );
	*trivial_irq_triggered = 0;
	fake_irq ( irq );
	if ( *trivial_irq_triggered != 1 ) {
		IRQ_DBG ( "Installation of trivial IRQ handler failed\n" );
		remove_trivial_irq_handler ();
		return 0;
	}
	IRQ_DBG ( "Trivial IRQ handler installed successfully\n" );
	*trivial_irq_triggered = 0;
	enable_irq ( irq );
	return 1;
}

int remove_trivial_irq_handler ( void ) {
	int removed = 0;
	segoff_t trivial_irq_handler_segoff = SEGOFF(trivial_irq_handler);

	if ( trivial_irq_installed_on == IRQ_NONE ) return 1;
	removed = remove_irq_handler ( trivial_irq_installed_on,
				       &trivial_irq_handler_segoff,
				       trivial_irq_chain,
				       trivial_irq_chain_to );
	if ( ! removed ) return 0;
	trivial_irq_installed_on = IRQ_NONE;

	return 1;
}

#ifdef DEBUG_IRQ
void dump_irq_status ( void ) {
	int irq = 0;
	
	for ( irq = 0; irq < 16; irq++ ) {
		if ( irq_enabled ( irq ) ) {
			printf ( "IRQ%d enabled, ISR at %hx:%hx\n", irq,
				 IRQ_VECTOR(irq)->segment,
				 IRQ_VECTOR(irq)->offset );
		}
	}
}
#endif
