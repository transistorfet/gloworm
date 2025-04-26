
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

#include <asm/irqs.h>
#include <kernel/bh.h>
#include <kernel/printk.h>
#include <kernel/signal.h>
#include <kernel/interrupts.h>


#define INTERRUPT_MAX		128

#define INT_FLAG_ENABLED	0x01

extern void exception_entry(void);

struct exception_stack_frame;
void fatal_error(struct exception_stack_frame *frame);
void handle_trap_1(void);
void handle_trace(void);

static struct irq_action action_table[INTERRUPT_MAX];

void init_interrupts(void)
{
	init_bh();

	extern void enter_handle_exception();
	for (short i = 0; i < INTERRUPT_MAX; i++) {
		action_table[i].irq = 0;
		action_table[i].flags = 0;
		action_table[i].handler = NULL;
	}

	//extern void enter_handle_trace();
	//set_interrupt(IV_TRACE, enter_handle_trace);

	arch_init_irqs();
}

void request_irq(irq_num_t irq, irq_handler_t handler, int flags)
{
	if (action_table[(short) irq].handler) {
		printk_safe("already an irq handler for %d, overwriting\n", irq);
	}
	action_table[(short) irq].flags = flags;
	action_table[(short) irq].handler = handler;
}

void free_irq(irq_num_t irq)
{
	action_table[(short) irq].flags = 0;
	action_table[(short) irq].handler = 0;
}

void enable_irq(irq_num_t irq)
{
	action_table[(short) irq].flags |= INT_FLAG_ENABLED;
}

void disable_irq(irq_num_t irq)
{
	action_table[(short) irq].flags &= ~INT_FLAG_ENABLED;
}

void do_irq(irq_num_t irq)
{
	//printk_safe("irq %d\n", irq);
	// TODO call the appropriate interrupt
	//extern irq_handler_t handle_serial_irq();
	//handle_serial_irq();

	struct irq_action *action = &action_table[(short) irq];
	//printk_safe("%d %d %x\n", irq, action->flags, action->handler);
	if (irq < INTERRUPT_MAX && action->handler && action->flags & INT_FLAG_ENABLED) {
		action->handler();
	}
}

