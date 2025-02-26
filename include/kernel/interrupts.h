
#ifndef _KERNEL_INTERRUPTS_H
#define _KERNEL_INTERRUPTS_H

typedef unsigned int irq_num_t;

typedef void (*irq_handler_t)();

struct irq_action {
	irq_num_t irq;
	int flags;
	irq_handler_t handler;
};

void init_interrupts();
void request_interrupt(irq_num_t irq, irq_handler_t handler, int flags);
void free_interrupt(irq_num_t irq);
void enable_irq(irq_num_t irq);
void disable_irq(irq_num_t irq);

#endif

