
#include <string.h>

#include <kconfig.h>
#include <asm/context.h>
#include <kernel/arch/context.h>
#include <kernel/proc/fork.h>
#include <kernel/proc/signal.h>
#include <kernel/proc/process.h>


struct sigcontext {
	int signum;
	sigset_t prev_mask;
};

void *arch_get_user_stackp(struct process *proc)
{
	void *usp;

	#if defined(CONFIG_M68K_USER_MODE)
	if (info_to_regs(&proc->task_info)->size == FULL_CONTEXT_SIZE) {
		return (void *) access_reg(&proc->task_info, regs.usp);
	} else if (proc == current_proc) {
		asm volatile("movec.l	%%usp, %0\n" : "=r" (usp) : );
		return usp;
	} else {
		printk("fuck, trying to read usp from different pid %d?\n", proc->pid);
		return NULL;
	}
	#else
	return proc->task_info->ksp;
	#endif
}

void arch_set_user_stackp(struct process *proc, void *usp)
{
	#if defined(CONFIG_M68K_USER_MODE)
	// TODO fix this, maybe make them normal functions?
	if (info_to_regs(&proc->task_info)->size == FULL_CONTEXT_SIZE) {
		access_reg(&proc->task_info, regs.usp) = (uint32_t) usp;
	} else if (proc == current_proc) {
		asm volatile("movec.l	%0, %%usp\n" : : "r" (usp));
	} else {
		printk("fuck, trying to write usp to different pid %d?\n", proc->pid);
	}
	#else
	proc->task_info.ksp = usp;
	#endif
}

void *arch_get_kernel_stackp(struct process *proc)
{
	return proc->task_info.ksp;
}

void arch_set_kernel_stackp(struct process *proc, void *ksp)
{
	proc->task_info.ksp = ksp;
}

int arch_init_task_info(struct process *proc)
{
	#if defined(CONFIG_M68K_USER_MODE)

	if (!proc->task_info.kernel_stack) {
		proc->task_info.kernel_stack = (char *) page_alloc_contiguous(KERNEL_STACK_SIZE);
		if (!proc->task_info.kernel_stack)
			return ENOMEM;
	}
	proc->task_info.ksp = proc->task_info.kernel_stack + KERNEL_STACK_SIZE;

	#else

	proc->task_info.ksp = NULL;

	#endif

	return 0;
}

int arch_release_task_info(struct process *proc)
{
	#if defined(CONFIG_M68K_USER_MODE)

	if (proc->task_info.kernel_stack) {
		page_free_contiguous((page_t *) proc->task_info.kernel_stack, KERNEL_STACK_SIZE);
	}

	#endif

	return 0;
}

int arch_add_process_context(struct process *proc, char *user_sp, void *entry)
{
	//PUSH_STACK(stack_pointer, void *) = _exit;
	user_sp = ((char *) user_sp) - sizeof(void *);
	*((void **) user_sp) = _exit;

	#if defined(CONFIG_M68K_USER_MODE)

	proc->task_info.ksp = proc->task_info.kernel_stack + KERNEL_STACK_SIZE;

	#else

	proc->task_info.ksp = user_sp;

	#endif

	proc->task_info.ksp = create_context(proc->task_info.ksp, entry, user_sp);

	return 0;
}

int arch_clone_task_info(struct process *parent_proc, struct process *proc, char *user_sp)
{
	#if defined(CONFIG_M68K_USER_MODE)
	// TODO should the stack be allocated here too, so it's a decision made by the arch whether to have two stacks or one?
	proc->task_info.ksp = proc->task_info.kernel_stack + (((char *) parent_proc->task_info.ksp) - parent_proc->task_info.kernel_stack);

	memcpy((char *) proc->task_info.kernel_stack, (char *) parent_proc->task_info.kernel_stack, KERNEL_STACK_SIZE);

	arch_set_user_stackp(proc, user_sp);

	#else

	proc->task_info.ksp = user_sp;

	#endif

	return 0;
}

int arch_add_signal_context(struct process *proc, int signum)
{
	void *ksp, *usp;
	struct sigcontext *context;

	// Save signal data on the stack for use by sigreturn
	context = (((struct sigcontext *) arch_get_kernel_stackp(proc)) - 1);
	context->signum = signum;
	context->prev_mask = proc->signals.blocked;
	proc->signals.blocked |= proc->signals.actions[signum - 1].sa_mask;

	// Push a return address that will call sigreturn() to the user stack
	usp = arch_get_user_stackp(proc);
	PUSH_STACK(usp, void *) = _sigreturn;
	// Push a fresh context onto the kernel stack
	// NOTE the user stack is not updated directly, but is instead added to the new context
	ksp = create_context((void *) context, proc->signals.actions[signum - 1].sa_handler, usp);
	arch_set_kernel_stackp(proc, ksp);

	return 0;
}

int arch_remove_signal_context(struct process *proc)
{
	void *ksp;
	struct sigcontext *context;

	ksp = drop_context(arch_get_kernel_stackp(proc));
	context = (struct sigcontext *) ksp;
	proc->signals.blocked = context->prev_mask;
	ksp = (((struct sigcontext *) ksp) + 1);
	arch_set_kernel_stackp(proc, ksp);

	return 0;
}

