/*
 * Basic support for controlling the 8259 Programmable Interrupt Controllers.
 *
 * Initially written by Michael Brown (mcb30).
 */

#ifndef PIC8259_H
#define PIC8259_H

/* For segoff_t */
#include <segoff.h>

/* 8259 register locations */
#define PIC1_ICW1 (0x20)
#define PIC1_OCW2 (0x20)
#define PIC1_OCW3 (0x20)
#define PIC1_IRR (0x20)
#define PIC1_ISR (0x20)
#define PIC1_ICW2 (0x21)
#define PIC1_ICW3 (0x21)
#define PIC1_ICW4 (0x21)
#define PIC1_IMR (0x21)
#define PIC2_ICW1 (0xa0)
#define PIC2_OCW2 (0xa0)
#define PIC2_OCW3 (0xa0)
#define PIC2_IRR (0xa0)
#define PIC2_ISR (0xa0)
#define PIC2_ICW2 (0xa1)
#define PIC2_ICW3 (0xa1)
#define PIC2_ICW4 (0xa1)
#define PIC2_IMR (0xa1)

/* Register command values */
#define OCW3_ID (0x08)
#define OCW3_READ_IRR (0x03)
#define OCW3_READ_ISR (0x02)

/* Macros to enable/disable IRQs */
#define IMR_REG(x) ( (x) < 8 ? PIC1_IMR : PIC2_IMR )
#define IMR_BIT(x) ( 1 << ( (x) % 8 ) )
#define irq_enabled(x) ( ( inb ( IMR_REG(x) ) & IMR_BIT(x) ) == 0 )
#define enable_irq(x) outb ( inb( IMR_REG(x) ) & ~IMR_BIT(x), IMR_REG(x) )
#define disable_irq(x) outb ( inb( IMR_REG(x) ) | IMR_BIT(x), IMR_REG(x) )

/* Utility macros to convert IRQ numbers to INT numbers and INT vectors  */
#define IRQ_INT(x) ( (x) < 8 ? (x)+0x08 : (x)-8+0x70 )
#define IRQ_VECTOR(x) ( (segoff_t*) phys_to_virt( 4 * IRQ_INT(x) ) )

/* Other constants */
typedef uint8_t irq_t;
#define IRQ_MAX (15)
#define IRQ_NONE (0xff)

/* Labels in assembly code (in pcbios.S)
 */
extern void _trivial_irq_handler ( void );
extern volatile uint16_t _trivial_irq_triggered;
extern segoff_t _trivial_irq_chain_to;
extern uint8_t _trivial_irq_chain;

/* Access to trivial_irq_triggered */
extern volatile uint16_t *trivial_irq_triggered;

/* Function prototypes
 */
int install_irq_handler ( irq_t irq, segoff_t *handler,
			  uint8_t *previously_enabled,
			  segoff_t *previous_handler );
int remove_irq_handler ( irq_t irq, segoff_t *handler,
			 uint8_t *previously_enabled,
			 segoff_t *previous_handler );
int install_trivial_irq_handler ( irq_t irq );
int remove_trivial_irq_handler ( void );
#ifdef DEBUG_IRQ
void dump_irq_status ( void );
#else
#define dump_irq_status()
#endif

#endif /* PIC8259_H */
