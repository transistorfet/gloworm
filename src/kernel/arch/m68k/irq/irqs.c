
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
#include <kernel/utils/math.h>

#include <asm/context.h>
#include <asm/exceptions.h>

#if defined(CONFIG_MMU)
#include <asm/mmu.h>
#endif

#define INTERRUPT_MAX		128

extern int kernel_reentries;
extern void enter_exception(void);
#if defined(CONFIG_MMU)
uint32_t recovery_stack;
#endif

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

	#if defined(CONFIG_MMU)
	recovery_stack = 0;
	#endif
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

void user_error(struct exception_frame *frame, int signal)
{
	log_error("\nError in pid %d at %x (status: %x, vector: %d)\n", current_proc->pid, frame->pc, frame->status, (frame->vector & 0xFFF) >> 2);
	printk("pid %d memory map:\n", current_proc->pid);
	memory_map_print_segments(current_proc->map);

	// After an exception, the `frame` is always pointing to the global kernel stack
	printk("Exception Frame: %x\n", frame);
	printk_dump((uint16_t *) frame, 128);

	#if defined(CONFIG_MMU)

	int error;
	char *code;
	struct get_page_result page;

	#if defined(CONFIG_LOG_LEVEL_DEBUG)
	struct mmu_root_pointer root_pointer;
	MMU_MOVE_FROM_CRP(root_pointer);
	log_debug("mmu: root pointer is %x\n", root_pointer.table);
	mmu_table_print(current_proc->map->root_table);
	#endif

	// The kernel stack is available through the process struct's task info
	char *ksp = arch_get_kernel_stackp(current_proc);
	printk("Process Kernel Stack (Context): %x\n", ksp);
	printk_dump((uint16_t *) ksp, 128);

	virtual_address_t usp = (virtual_address_t) arch_get_user_stackp(current_proc);
	error = mmu_table_get_page(current_proc->map->root_table, rounddown(usp, PAGE_SIZE), &page);
	printk("User Stack: %x (page %x)\n", usp, page.phys);
	if (usp && error < 0) {
		printk_dump((uint16_t *) &(((char *) page.phys)[alignment_offset(usp, page.size)]), 128);
	}

	error = mmu_table_get_page(current_proc->map->root_table, (virtual_address_t) rounddown(frame->pc, PAGE_SIZE), &page);
	code = (char *) (page.phys + alignment_offset((uintptr_t) frame->pc, page.size));
	printk("\nCode: %x (phys: %x)\n", frame->pc, code);
	printk_dump((uint16_t *) code, 48);

	#else

	char *ksp = arch_get_kernel_stackp(current_proc);
	printk("Process Kernel+User Stack: %x\n", ksp);
	print_stack(frame, (void *) ksp);

	printk("\nCode: %x\n", frame->pc);
	printk_dump((uint16_t *) frame->pc, 48);

	#endif

	dispatch_signal(current_proc, SIGABRT);
}

void fatal_error(struct exception_frame *frame)
{
	void console_prepare_for_panic(void);

	console_prepare_for_panic();

	log_fatal("\n\nFatal Error at %x while pid %d running (status: %x, vector: %x). Halting...\n", frame->pc, current_proc->pid, frame->status, (frame->vector & 0xFFF) >> 2);

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
	// Exception occurred inside a user program
	switch ((frame->vector & 0xFFF) >> 2) {
		case IRQ_BUS_ERROR:
			page_fault_handler(frame);
			break;

		default: {
			extern int kernel_reentries;
			if (kernel_reentries <= 1) {
				user_error(frame, SIGABRT);
			} else {
				fatal_error(frame);
				asm volatile("stop #0x2700\n");
			}
			break;
		}
	}
}

void handle_trace(struct exception_frame *frame, struct syscall_registers *regs)
{
	printk_direct("Trace pc=%x sp=%x (pc)=%04x\n", frame->pc, frame, *((uint16_t *) frame->pc));

	if (frame->pc == 0x106328 || frame->pc == 0x106372) {
		printk_direct("a0=%x d0=%x\n", regs->a0, regs->d0);
	}
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
		log_debug("bus error: %s %c fc=%d addr=%x ssw=%x mmusr=%x pid=%d pc=%x\n", ff && df ? "i+d" : ff ? "i" : "d", rwm_type, ssw & 0x7, fault_addr, ssw, mmu_sr, current_proc->pid, frame->pc);

		if (ff && df) {
			log_error("the infamous ff+df cycle fault\n");
		} else if (ff) {
			// An instruction fault has occurred, and if there's already a page mapped to that address,
			// it will be returned when we RTE.  It's being recorded in a Program Space function codes, and
			// was previously only recorded in the Data Space function codes
			error = mmu_table_get_page(current_proc->map->root_table, rounddown(fault_addr, PAGE_SIZE), NULL);
			if (!error) {
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
		return;
	}

fail:
	if (kernel_reentries == 2 && recovery_stack) {
		int stack = recovery_stack;
		recovery_stack = 0;
		kernel_reentries -= 1;
		user_error(frame, SIGSEGV);
		asm volatile(
			"move.l	%0, %%sp\n"
			"bra.l	restore_context\n"
			: : "g" (stack)
		);
	}

	log_error("fatal bus error: %s %c fc=%d addr=%x ssw=%x mmusr=%x pid=%d pc=%x\n", ff && df ? "i+d" : ff ? "i" : "d", rwm_type, ssw & 0x7, fault_addr, ssw, mmu_sr, current_proc->pid, frame->pc);
	memory_map_print_segments(current_proc->map);
	#endif

	if (kernel_reentries <= 1) {
		user_error(frame, SIGSEGV);
	} else {
		fatal_error(frame);
		asm volatile("stop #0x2700\n");
	}
}

