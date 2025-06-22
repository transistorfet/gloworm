
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

#include <asm/irqs.h>
#include <kernel/printk.h>
#include <kernel/irq/bh.h>
#include <kernel/irq/action.h>
#include <kernel/proc/signal.h>
#include <kernel/proc/process.h>

#include <asm/context.h>
#include <asm/exceptions.h>

#define INTERRUPT_MAX		128

typedef void (*m68k_irq_handler_t)();

extern void enter_exception(void);

void fatal_error(struct context *frame);
void handle_trace(void);

static m68k_irq_handler_t vector_table[INTERRUPT_MAX];

void arch_init_irqs(void)
{
	extern void enter_exception();
	extern void enter_syscall();
	extern void enter_irq();

	// Processor exceptions
	for (short i = IRQ_BUS_ERROR; i < IRQ_AUTOVEC1; i++)
		vector_table[i] = enter_exception;

	// Autovec interrupts
	for (short i = IRQ_AUTOVEC1; i <= IRQ_AUTOVEC7; i++)
		vector_table[i] = enter_irq;

	// Trap instructions handlers
	for (short i = IRQ_TRAP0; i <= IRQ_TRAP15; i++)
		vector_table[i] = enter_syscall;

	// Reserved interrupts vectors
	for (short i = IRQ_TRAP15 + 1; i < IRQ_USER_START; i++)
		vector_table[i] = enter_exception;

	// User interrupts
	for (short i = IRQ_USER_START; i < IRQ_USER_MAX; i++)
		vector_table[i] = enter_irq;

	extern void enter_handle_trace();
	vector_table[IRQ_TRACE] = enter_handle_trace;

	// Load the VBR register with the address of our vector table
	asm volatile("movec	%0, %%vbr\n" : : "r" (vector_table));
}

// TODO actually this isn't needed...
void arch_set_irq_handler(irq_num_t irq, m68k_irq_handler_t handler)
{
	vector_table[(short) irq] = handler;
}

/**
 * Interrupt Handlers
 */

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


void print_stack(struct exception_frame *frame)
{
	uint16_t *code = (uint16_t *) frame->pc;
	uint16_t *stack = (uint16_t *) frame;

	// Dump stack
	printk("Stack: %x\n", stack);
	printk_dump(stack, 48);

	// Dump code where the error occurred
	printk("\nCode: %x\n", code);
	printk_dump(code, 48);
}

void user_error(struct context *context)
{
	extern struct process *current_proc;

	struct exception_frame *frame = &context->frame;

	log_error("\nError in pid %d at %x (status: %x, vector: %x)\n", current_proc->pid, frame->pc, frame->status, (frame->vector & 0xFFF) >> 2);
	print_process_segments(current_proc);
	print_stack(frame);

	dispatch_signal(current_proc, SIGKILL);
}

void fatal_error(struct context *context)
{
	struct exception_frame *frame = &context->frame;
	void console_prepare_for_panic(void);

	console_prepare_for_panic();

	log_fatal("\n\nFatal Error at %x (status: %x, vector: %x). Halting...\n", frame->pc, frame->status, (frame->vector & 0xFFF) >> 2);

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

void handle_exception(void)
{
	struct context *context;

	GET_FRAME(context);

	extern int kernel_reentries;
	if (kernel_reentries < 1) {
		user_error(context);
		// TODO this is temporary until you sort out how to return... basically need to call schedule and then restore_context, but the acutal restore_context could be in syscall_entry.S
		asm volatile("stop #0x2700\n");
	} else {
		fatal_error(context);
		asm volatile("stop #0x2700\n");
	}
}

INTERRUPT_ENTRY(handle_trace);

__attribute__((interrupt)) void handle_trace()
{
	struct context *context;

	GET_FRAME(context);

	log_trace("Trace %x (%x)\n", context->frame.pc, context->frame.pc);
}
