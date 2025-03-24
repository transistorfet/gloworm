
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
	for (short i = 2; i < INTERRUPT_MAX; i++) {
		action_table[i].irq = 0;
		action_table[i].flags = 0;
		action_table[i].handler = NULL;
	}

	//extern void enter_handle_trace();
	//set_interrupt(IV_TRACE, enter_handle_trace);

	init_irqs();
}

void request_irq(irq_num_t irq, irq_handler_t handler, int flags)
{
	action_table[(short) irq].flags = flags;
	action_table[(short) irq].handler = handler;
}

void free_irq(irq_num_t irq)
{

}

void enable_irq(irq_num_t irq)
{

}

void disable_irq(irq_num_t irq)
{

}

/*
struct exception_stack_frame {
	uint16_t status;
	uint16_t *pc;
	uint16_t vector;
};

void print_stack(struct exception_stack_frame *frame)
{
	// Dump stack
	printk_safe("Stack: %x\n", frame);
	for (short i = 0; i < 48; i++) {
		printk_safe("%04x ", ((uint16_t *) frame)[i]);
		if ((i & 0x7) == 0x7)
			printk_safe("\n");
	}

	// Dump code where the error occurred
	printk_safe("\nCode:\n");
	for (short i = 0; i < 48; i++) {
		printk_safe("%04x ", frame->pc[i]);
		if ((i & 0x7) == 0x7)
			printk_safe("\n");
	}
}
*/
