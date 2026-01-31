
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

#if defined(CONFIG_MMU)
#include <asm/mmu.h>
#endif

#define INTERRUPT_MAX		128

extern void enter_exception(void);

void enter_trace(struct exception_frame frame);
static void page_fault_handler(struct exception_frame *frame);

static bare_irq_handler_t vector_table[INTERRUPT_MAX];

void arch_init_irqs(void)
{
	extern void enter_exception();
	extern void enter_syscall();
	extern void enter_irq();

	// Processor exceptions
	for (short i = IRQ_BUS_ERROR; i < IRQ_AUTOVEC1; i++) {
		vector_table[i] = enter_exception;
	}

	// Autovec interrupts
	for (short i = IRQ_AUTOVEC1; i <= IRQ_AUTOVEC7; i++) {
		vector_table[i] = enter_irq;
	}

	// Trap instructions handlers
	for (short i = IRQ_TRAP0; i <= IRQ_TRAP15; i++) {
		vector_table[i] = enter_syscall;
	}

	// Reserved interrupts vectors
	for (short i = IRQ_TRAP15 + 1; i < IRQ_USER_START; i++) {
		vector_table[i] = enter_exception;
	}

	// User interrupts
	for (short i = IRQ_USER_START; i < IRQ_USER_MAX; i++) {
		vector_table[i] = enter_irq;
	}

	vector_table[IRQ_TRACE] = (bare_irq_handler_t) enter_trace;

	// Load the VBR register with the address of our vector table
	asm volatile("movec	%0, %%vbr\n" : : "r" (vector_table));
}

// TODO actually this isn't needed...
void arch_set_irq_handler(irq_num_t irq, bare_irq_handler_t handler)
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
	log_error("\nError in pid %d at %x (status: %x, vector: %d)\n", current_proc->pid, frame->pc, frame->status, (frame->vector & 0xFFF) >> 2);
	printk("pid %d memory map:\n", current_proc->pid);
	memory_map_print_segments(current_proc->map);
	mmu_table_print(current_proc->map->root_table);

	#if defined(CONFIG_MMU)

	// After an exception, the `frame` is always pointing to the global kernel stack
	printk("Exception Frame: %x\n", frame);
	printk_dump((uint16_t *) frame, 128);

	// The kernel stack is available through the process struct's task info
	char *ksp = arch_get_kernel_stackp(current_proc);
	printk("Process Kernel Stack (Context): %x\n", ksp);
	printk_dump((uint16_t *) ksp, 128);

	char *usp = arch_get_user_stackp(current_proc);
	if (usp) {
		printk("User Kernel Stack: %x\n", usp);
		printk_dump((uint16_t *) usp, 128);
	}

	char *code = (char *) mmu_table_get_page(current_proc->map->root_table, (virtual_address_t) frame->pc & ~(PAGE_SIZE - 1), 1);
	code += (uintptr_t) frame->pc & (PAGE_SIZE - 1);
	printk("\nCode: %x (phys: %x)\n", frame->pc, code);
	printk_dump((uint16_t *) code, 48);

	#else

	print_stack(frame, (void *) frame->pc);

	#endif

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

static void page_fault_handler(struct exception_frame *frame)
{
	#if defined(CONFIG_MMU)

	int error = 0;
	uint16_t ssw;
	uint16_t mmu_sr;
	uintptr_t fault_addr;

	fault_addr = frame->formatb.fault_addr;
	ssw = frame->formatb.ssw;
	int df = ssw & SSW_DATA_FAULT;
	int ff = (ssw & SSW_FAULT_STAGE_C) || (ssw & SSW_FAULT_STAGE_B);
	char rwm_type = !df ? 'r' : (!(ssw & SSW_READ_WRITE) ? 'w' : ((ssw & SSW_READ_MODIFY_WRITE) ? 'm' : 'r'));
	int write_flag = df && (!(ssw & SSW_READ_WRITE) || (ssw & SSW_READ_MODIFY_WRITE));

	if (current_proc && current_proc->map) {
		if (rwm_type == 'r') {
			asm volatile(
				"ptestr %2, %1@, #7\n"
				"pmove %%psr, %0\n"
				: "=m" (mmu_sr) : "a" (fault_addr), "d" (ssw & 0x7)
			);
		} else {
			asm volatile(
				"ptestw %2, %1@, #7\n"
				"pmove %%psr, %0\n"
				: "=m" (mmu_sr) : "a" (fault_addr), "d" (ssw & 0x7)
			);
		}
		log_info("bus error: %s %c fc=%d addr=%x ssw=%x mmusr=%x pid=%d pc=%x\n", ff && df ? "i+d" : ff ? "i" : "d", rwm_type, ssw & 0x7, fault_addr, ssw, mmu_sr, current_proc->pid, frame->pc);

		if (ff && df) {
			log_error("the infamous ff+df cycle fault\n");
		} else if (ff) {
			// An instruction fault has occurred, and if there's already a page mapped to that address,
			// it will be returned when we RTE.  It's being recorded in a Program Space function codes, and
			// was previously only recorded in the Data Space function codes
			physical_address_t existing_page = mmu_table_get_page(current_proc->map->root_table, fault_addr & ~(PAGE_SIZE - 1), 0);
			if (existing_page) {
				return;
			}

			// The page doesn't already exist, so load it
			error = memory_map_load_page_at(current_proc->map, fault_addr, 0);
			if (error < 0) {
				goto fail;
			}
		} else if (df) {
			error = memory_map_load_page_at(current_proc->map, fault_addr, write_flag);
			if (error < 0) {
				goto fail;
			}
		} else {
			log_error("segfault\n");
			goto fail;
		}

		//MMU_FLUSH_ALL();
		//if (write_flag) {
		//	asm volatile(
		//		"ploadw	%1, %0@\n"
		//		: : "a" (fault_addr), "d" (ssw)
		//	);
		//}
		//else {
		//	asm volatile(
		//		"ploadr	%1, %0@\n"
		//		: : "a" (fault_addr), "d" (ssw)
		//	);
		//}
		return;
	}

fail:
	log_error("fatal bus error: %s %c fc=%d addr=%x ssw=%x mmusr=%x pid=%d pc=%x\n", ff && df ? "i+d" : ff ? "i" : "d", rwm_type, ssw & 0x7, fault_addr, ssw, mmu_sr, current_proc->pid, frame->pc);
	memory_map_print_segments(current_proc->map);
	#endif

	user_error(frame);
}

