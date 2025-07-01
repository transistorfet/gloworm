
#ifndef _ASM_M68K_CONTEXT_H
#define _ASM_M68K_CONTEXT_H

#include <stdint.h>
#include <kconfig.h>
#include <asm/exceptions.h>
#include <kernel/printk.h>

#if defined(CONFIG_MMU)
#include <asm/mmu.h>
#endif

/// The registers stored on the stack as part of a saved context
struct context_registers {
	// Full Context
	uint32_t usp;
	uint32_t d6;
	uint32_t d7;
	uint32_t a0;
	uint32_t a1;
	uint32_t a2;
	uint32_t a3;
	uint32_t a4;
	uint32_t a5;
	uint32_t a6;

	// TODO need to make breaking change to syscall registers
	// Short Context
	uint32_t d0;
	uint32_t d1;
	uint32_t d2;
	uint32_t d3;
	uint32_t d4;
	uint32_t d5;
};


/// The full context on the stack, including the exception frame
struct context {
	uint32_t size;
	struct context_registers regs;
	struct exception_frame frame;
};

/// The info stored in the process struct about the task's state
///
/// This is where the stack pointer is stored, which has the saved context on top,
/// but would also be where floating point registers would be saved
struct arch_task_info {
	/// The kernel stack pointer for this process
	/// If user mode is not used, this will also be the user stack pointer
	void *ksp;

	#if defined(CONFIG_M68K_USER_MODE)
	/// A pointer to the start of the kernel stack for this process
	void* kernel_stack;
	#endif
};

#define PUSH_STACK(stack_pointer, ttype) \
	(stack_pointer) = ((char *) (stack_pointer)) - sizeof((ttype));	\
	*(((ttype) *) (stack_pointer))


#define info_to_regs(info)	((struct context *) (info->ksp))

#define access_reg(info, name)	\
	info_to_regs(info)->name

// TODO this also defined in the assembly file =/
#define FULL_CONTEXT_SIZE	64

static inline void *get_user_stackp(struct arch_task_info *info)
{
	#if defined(CONFIG_M68K_USER_MODE)
	if (info_to_regs(info)->size == FULL_CONTEXT_SIZE) {
		return (void *) access_reg(info, regs.usp);
	} else {
		printk("fuck, trying to write usp to current?\n");
	}
	#else
	return info->ksp;
	#endif
}

static inline void set_user_stackp(struct arch_task_info *info, void *usp)
{
	#if defined(CONFIG_M68K_USER_MODE)
	if (info_to_regs(info)->size == FULL_CONTEXT_SIZE) {
		access_reg(info, regs.usp) = (uint32_t) usp;
	} else {
		printk("fuck, trying to write usp to current?\n");
	}
	#else
	info->ksp = usp;
	#endif
}

static inline void *get_kernel_stackp(struct arch_task_info *info)
{
	return info->ksp;
}

static inline void set_kernel_stackp(struct arch_task_info *info, void *ksp)
{
	info->ksp = ksp;
}

#endif

