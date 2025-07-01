
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

int arch_reinit_task_info(struct process *proc, void *user_sp, void *entry)
{
	//PUSH_STACK(stack_pointer, void *) = _exit;
	user_sp = ((char *) user_sp) - sizeof(void *);
	*((void **) user_sp) = _exit;

	#if defined(CONFIG_M68K_USER_MODE)

	if (!proc->task_info.kernel_stack) {
		proc->task_info.kernel_stack = page_alloc_contiguous(KERNEL_STACK_SIZE);
		if (!proc->task_info.kernel_stack)
			return ENOMEM;
	}
	proc->task_info.ksp = proc->task_info.kernel_stack + KERNEL_STACK_SIZE;

	#else

	proc->task_info.ksp = user_sp;

	#endif

	proc->task_info.ksp = create_context(proc->task_info.ksp, entry, NULL, user_sp);

	return 0;
}

int arch_release_task_info(struct process *proc)
{
	#if defined(CONFIG_M68K_USER_MODE)

	if (proc->task_info.kernel_stack) {
		page_free_contiguous(proc->task_info.kernel_stack, KERNEL_STACK_SIZE);
	}

	#endif

	return 0;
}

int arch_clone_task_info(struct process *parent_proc, struct process *proc, char *user_sp)
{
	#if defined(CONFIG_M68K_USER_MODE)
	// TODO should the stack be allocated here too, so it's a decision made by the arch whether to have two stacks or one?
	proc->task_info.ksp = proc->task_info.kernel_stack + (parent_proc->task_info.ksp - parent_proc->task_info.kernel_stack);

	memcpy((char *) proc->task_info.kernel_stack, (char *) parent_proc->task_info.kernel_stack, KERNEL_STACK_SIZE);

	((struct context *) proc->task_info.ksp)->regs.usp = (uint32_t) user_sp;

	#else

	proc->task_info.ksp = user_sp;

	#endif

	return 0;
}

int arch_add_signal_context(struct process *proc, int signum)
{
	void *stack_pointer;
	struct sigcontext *context;

	// Save signal data on the stack for use by sigreturn
	context = (((struct sigcontext *) get_kernel_stackp(&proc->task_info)) - 1);
	context->signum = signum;
	context->prev_mask = proc->signals.blocked;
	proc->signals.blocked |= proc->signals.actions[signum - 1].sa_mask;

	// Push a fresh context onto the stack, which will run the handler and then call sigreturn()
	stack_pointer = (void *) context;
	stack_pointer = ((char *) stack_pointer) - sizeof(void *);
	*((void **) stack_pointer) = _sigreturn;
	stack_pointer = create_context(stack_pointer, proc->signals.actions[signum - 1].sa_handler, NULL, NULL);
	set_kernel_stackp(&proc->task_info, stack_pointer);

	return 0;
}

int arch_remove_signal_context(struct process *proc)
{
	void *stack_pointer;
	struct sigcontext *context;

	stack_pointer = drop_context(get_kernel_stackp(&proc->task_info));
	context = (struct sigcontext *) stack_pointer;
	proc->signals.blocked = context->prev_mask;
	stack_pointer = (((struct sigcontext *) stack_pointer) + 1);
	set_kernel_stackp(&proc->task_info, stack_pointer);

	return 0;
}

