
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

#include "../../proc/process.h"		// TODO this should be <kernel/process.h> or maybe convert to tasks/threads first?

#define INTERRUPT_MAX		128

typedef void (*m68k_irq_handler_t)();

extern void exception_entry(void);

struct exception_stack_frame;
void fatal_error(struct exception_stack_frame *frame);
void handle_trap_1(void);
void handle_trace(void);

static m68k_irq_handler_t vector_table[INTERRUPT_MAX];

void init_irqs(void)
{
	extern void enter_handle_exception();
	for (short i = 2; i < INTERRUPT_MAX; i++)
		vector_table[i] = enter_handle_exception;

	extern void enter_handle_trace();
	set_irq_handler(IRQ_TRACE, enter_handle_trace);

	// Load the VBR register with the address of our vector table
	asm volatile("movec	%0, %%vbr\n" : : "r" (vector_table));
}

void set_irq_handler(irq_num_t irq, m68k_irq_handler_t handler)
{
	vector_table[(short) irq] = handler;
}

void do_irq(int irq) {
	// TODO call the appropriate interrupt
	extern m68k_irq_handler_t handle_serial_irq();
	handle_serial_irq();
}

/**
 * Interrupt Handlers
 */

struct exception_stack_frame {
	uint16_t status;
	uint16_t *pc;
	uint16_t irq;
};

#define INTERRUPT_ENTRY(name)				\
__attribute__((noreturn)) void enter_##name()		\
{							\
	asm(						\
	"move.l	%sp, %a5\n"				\
	"bra	" #name "\n"				\
	);						\
	__builtin_unreachable();			\
}

#define GET_FRAME(frame_ptr)				\
	asm("move.l	%%a5, %0\n" : "=r" (frame_ptr))


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

void user_error(struct exception_stack_frame *frame)
{
	extern struct process *current_proc;

	printk_safe("\nError in pid %d at %x (status: %x, vector: %x)\n", current_proc->pid, frame->pc, frame->status, (frame->irq & 0xFFF) >> 2);
	print_proc_segments(current_proc);
	print_stack(frame);

	dispatch_signal(current_proc, SIGKILL);
}

void fatal_error(struct exception_stack_frame *frame)
{
	prepare_for_panic();

	printk_safe("\n\nFatal Error at %x (status: %x, vector: %x). Halting...\n", frame->pc, frame->status, (frame->irq & 0xFFF) >> 2);

	print_stack(frame);

	// Jump to the monitor to allow debugging
	asm volatile(
	"move.l	#0, %a0\n"
	"movec	%a0, %vbr\n"
	//"move.l	(%a0)+, %sp\n"		// TODO it's safer to reset the stack pointer for the monitor, but if we keep the old one, it's easier to debug fatals
	"move.l	#4, %a0\n"
	"jmp	(%a0)\n"
	);

	__builtin_unreachable();
}

INTERRUPT_ENTRY(handle_exception);

__attribute__((interrupt)) void handle_exception(void)
{
	DISABLE_INTS();

	struct exception_stack_frame *frame;

	GET_FRAME(frame);

	extern int kernel_reentries;
	if (kernel_reentries < 1) {
		user_error(frame);
	} else {
		fatal_error(frame);
	}
}

INTERRUPT_ENTRY(handle_fatal_error);

__attribute__((interrupt)) void handle_fatal_error()
{
	DISABLE_INTS();

	struct exception_stack_frame *frame;

	GET_FRAME(frame);

	fatal_error(frame);
	asm volatile("stop #0x2700\n");
}

INTERRUPT_ENTRY(handle_trace);

__attribute__((interrupt)) void handle_trace()
{
	struct exception_stack_frame *frame;

	GET_FRAME(frame);

	printk_safe("Trace %x (%x)\n", frame->pc, *frame->pc);
}
