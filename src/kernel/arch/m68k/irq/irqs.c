
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

#include <asm/irqs.h>
#include <kernel/printk.h>
#include <kernel/irq/bh.h>
#include <kernel/irq/action.h>
#include <kernel/arch/context.h>
#include <kernel/proc/signal.h>
#include <kernel/proc/process.h>

#include <asm/context.h>
#include <asm/exceptions.h>

#define INTERRUPT_MAX		128

typedef void (*m68k_irq_handler_t)();

extern void enter_exception(void);

void enter_trace(struct exception_frame frame);
static void page_fault_handler(struct exception_frame *frame);

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

	vector_table[IRQ_TRACE] = (m68k_irq_handler_t) enter_trace;

	// Load the VBR register with the address of our vector table
	asm volatile("movec	%0, %%vbr\n" : : "r" (vector_table));
}

// TODO actually this isn't needed...
void arch_set_irq_handler(irq_num_t irq, m68k_irq_handler_t handler)
{
	vector_table[(short) irq] = handler;
}

void print_stack(void *stack, void *code)
{
	// Dump stack
	printk("Stack: %x\n", stack);
	printk_dump(stack, 128);

	// Dump code where the error occurred
	printk("\nCode: %x\n", code);
	if (code) {
		printk_dump(code, 48);
	}
}

///////////////////////////
// Exception Handlers
///////////////////////////

extern struct process *current_proc;

void user_error(struct exception_frame *frame)
{
	page_t *page;

	log_error("\nError in pid %d at %x (status: %x, vector: %d)\n", current_proc->pid, frame->pc, frame->status, (frame->vector & 0xFFF) >> 2);
	printk("pid %d memory map:\n", current_proc->pid);
	memory_map_print_segments(current_proc->map);
	// TODO this is for when the user-mode PC is pointing to its private virtual address space
	//#if defined(CONFIG_MMU)
	//page = mmu_table_get_page(current_proc->map->root_table, frame->pc);
	//#else
	page = (page_t *) frame->pc;
	//#endif
	print_stack(frame, page);

	dispatch_signal(current_proc, SIGABRT);
}

void fatal_error(struct exception_frame *frame)
{
	void console_prepare_for_panic(void);

	console_prepare_for_panic();

	log_fatal("\n\nFatal Error at %x (status: %x, vector: %x). Halting...\n", frame->pc, frame->status, (frame->vector & 0xFFF) >> 2);

	print_stack(frame, (void *) frame->pc);

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

void handle_exception(struct exception_frame *frame)
{
	if (((frame->vector & 0xFFF) >> 2) == IRQ_BUS_ERROR) {
		page_fault_handler(frame);
	} else {
		extern int kernel_reentries;
		if (kernel_reentries <= 1) {
			user_error(frame);
		} else {
			fatal_error(frame);
			asm volatile("stop #0x2700\n");
		}
	}
}

__attribute__((interrupt)) void enter_trace(struct exception_frame frame)
{
	log_trace("Trace %x (%x)\n", frame.pc, frame.pc);
}

#include <asm/mmu.h>
static void page_fault_handler(struct exception_frame *frame)
{
	int error = 0;

	#if defined(CONFIG_MMU)

	log_error("page fault @ %x\n", frame->formatb.fault_addr);
	if (current_proc && current_proc->map) {
		//printk("printing table %x\n", current_proc->map->root_table);
		//mmu_table_print(current_proc->map->root_table);
		error = memory_map_load_page_at(current_proc->map, frame->formatb.fault_addr);
		if (error < 0) {
			user_error(frame);
		}
	} else {
		user_error(frame);
	}

	#else

	user_error(frame);

	#endif
}

