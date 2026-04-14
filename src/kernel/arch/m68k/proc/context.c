
#include <string.h>
#include <signal.h>

#include <kconfig.h>
#include <asm/context.h>
#include <kernel/arch/context.h>
#include <kernel/proc/fork.h>
#include <kernel/proc/signal.h>
#include <kernel/proc/process.h>
#include <kernel/utils/iovec.h>


struct sigcontext {
	int signum;
	sigset_t prev_mask;
};


#if !defined(CONFIG_M68K_USER_MODE)
static inline int clone_context(struct process *parent_proc, struct process *proc, char **child_user_sp);
#endif

#if !defined(CONFIG_MMU)
char **adjust_string_array(char **source, char *parent_stack_start, char *new_stack_start);
static inline int copy_stack(struct process *parent_proc, struct process *proc);
#endif


void *arch_get_user_stackp(struct process *proc)
{
	#if defined(CONFIG_M68K_USER_MODE)
	void *usp;

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
	return proc->task_info.ksp;
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

	if (!proc->task_info.kernel_stack_start) {
		proc->task_info.kernel_stack_start = (char *) page_alloc_contiguous(KERNEL_STACK_SIZE);
		if (!proc->task_info.kernel_stack_start) {
			return ENOMEM;
		}
	}
	proc->task_info.ksp = proc->task_info.kernel_stack_start + KERNEL_STACK_SIZE;

	#else

	proc->task_info.ksp = NULL;

	#endif

	return 0;
}

int arch_release_task_info(struct process *proc)
{
	#if defined(CONFIG_M68K_USER_MODE)

	if (proc->task_info.kernel_stack_start) {
		page_free_contiguous((physical_address_t) proc->task_info.kernel_stack_start, KERNEL_STACK_SIZE);
	}

	#endif

	return 0;
}

int arch_add_process_context(struct process *proc, char *user_sp, void *entry)
{
	// Leave a space on the stack for the return address, so that the stack arguments are where they're expected
	user_sp -= sizeof(void *);

	#if defined(CONFIG_M68K_USER_MODE)

	proc->task_info.ksp = proc->task_info.kernel_stack_start + KERNEL_STACK_SIZE;

	#else

	// Add the user exit stub to the stack in case the user program attempts to return
	*((char **) user_sp) = (char *) _user_exit;
	proc->task_info.ksp = user_sp;

	#endif

	proc->task_info.ksp = create_context(proc->task_info.ksp, entry, user_sp, 0x0000);

	return 0;
}

int arch_clone_task_info(struct process *parent_proc, struct process *proc, char *user_sp)
{
	#if defined(CONFIG_M68K_USER_MODE)

	// Clone the kernel stack, if there is one
	memcpy((char *) proc->task_info.kernel_stack_start, (char *) parent_proc->task_info.kernel_stack_start, KERNEL_STACK_SIZE);
	proc->task_info.ksp = proc->task_info.kernel_stack_start + (((char *) parent_proc->task_info.ksp) - parent_proc->task_info.kernel_stack_start);

	#endif

	#if defined(CONFIG_MMU)

	// If no user stack is provided, then we only need to copy the parent's user stack pointer, and
	// the MMU's copy function, which enables copy-on-write, will clone the stack for us on demand
	if (!user_sp) {
		user_sp = arch_get_user_stackp(parent_proc);
	}

	arch_set_user_stackp(proc, user_sp);

	#else

	if (!user_sp) {
		int error;

		// If no user stack is provided, then we need to copy the parent's user stack
		error = copy_stack(parent_proc, proc);
		if (error < 0) {
			return error;
		}

		user_sp = (char *) proc->map->stack_end - (parent_proc->map->stack_end - (uintptr_t) arch_get_user_stackp(parent_proc));
		if (!user_sp) {
			return EFAULT;
		}
	} else {
		#if !defined(CONFIG_M68K_USER_MODE)
		// If a user stack was provided, then we need to copy the context on the parent's user stack to this stack,
		// so that we'll restore the context correctly the next time we switch to the child process
		clone_context(parent_proc, proc, &user_sp);
		#endif
	}

	#endif

	#if defined(CONFIG_M68K_USER_MODE)

	arch_set_user_stackp(proc, user_sp);

	#else

	proc->task_info.ksp = user_sp;

	#endif

	return 0;
}

int arch_add_signal_context(struct process *proc, int signum)
{
	int error;
	void *ksp, *usp;
	size_t iter_size;
	struct kvec kvec[3];
	struct iovec_iter iter;
	sigrestorer_t restorer;
	struct sigcontext *context;

	// Get the restorer function pointer, or raise an error if one is not set
	if (proc->signals.actions[signum - 1].sa_flags & SA_RESTORER) {
		restorer = proc->signals.actions[signum - 1].sa_restorer;
	} else {
		#if !defined(CONFIG_M68K_USER_MODE)
		restorer = _user_sigreturn;
		#else
		log_error("attempted to call a signal handler with no restorer\n");
		error = EINVAL;
		goto fail;
		#endif
	}

	// Save signal data on the kernel stack for use by sigreturn
	context = (((struct sigcontext *) arch_get_kernel_stackp(proc)) - 1);
	context->signum = signum;
	context->prev_mask = proc->signals.blocked;
	proc->signals.blocked |= proc->signals.actions[signum - 1].sa_mask;

	// Push to the user stack the signum argument, and the return address that will call sigreturn()
	iter_size = 0x20;
	usp = arch_get_user_stackp(proc);

	error = iovec_iter_load_pages_iter(proc->map, &iter, kvec, 3, (virtual_address_t) (usp - iter_size), iter_size, 1);
	if (error < 0) {
		goto fail;
	}

	error = iovec_iter_seek(&iter, 0, SEEK_END);
	if (error < 0) {
		goto fail;
	}

	error = iovec_iter_push_back(&iter, &signum, sizeof(void *));
	if (error < 0) {
		goto fail;
	}

	error = iovec_iter_push_back(&iter, &restorer, sizeof(void *));
	if (error < 0) {
		goto fail;
	}

	usp -= iovec_iter_remaining(&iter);

	// Push a fresh context onto the kernel stack
	// NOTE the user stack is not updated directly, but is instead added to the new context
	ksp = create_context((void *) context, proc->signals.actions[signum - 1].sa_handler, usp, 0x0000);
	arch_set_kernel_stackp(proc, ksp);

	return 0;

fail:
	return error;
}

int arch_remove_signal_context(struct process *proc)
{
	void *ksp;
	int signum;
	struct sigcontext *context;

	// Drop the handler context
	ksp = drop_context(arch_get_kernel_stackp(proc));

	// Use the sigcontext on the process's kernel stack to restore the previous signal state
	context = (struct sigcontext *) ksp;
	signum = context->signum;
	proc->signals.blocked = context->prev_mask;
	ksp = (((struct sigcontext *) ksp) + 1);
	arch_set_kernel_stackp(proc, ksp);

	return signum;
}

void arch_extended_switch_context(struct process *previous_proc, struct process *next_proc)
{
	#if defined(CONFIG_MMU)
	mmu_table_switch(next_proc->map->root_table);
	#endif
}

#if !defined(CONFIG_M68K_USER_MODE)
// Copy the process context on the parent's stack to the user stack
//
// This is meant to be used when a thread is started, and an alternate stack is provided rather
// than cloning the stack.  In order to correctly return to the child process (which only has a
// single user/kernel stack), the context must be copied
static inline int clone_context(struct process *parent_proc, struct process *proc, char **child_user_sp)
{
	size_t size;
	char *parent_user_sp;
	struct exception_frame *frame;

	parent_user_sp = arch_get_user_stackp(parent_proc);
	size = *((uint32_t *) parent_user_sp);
	frame = (struct exception_frame *) &parent_user_sp[size];
	size += sizeof(int) + exception_frame_size(frame);
	*child_user_sp -= size;
	memcpy(*child_user_sp, parent_user_sp, size);
	return 0;
}
#endif

#if !defined(CONFIG_MMU)
// Copy the parent's user stack to a new memory location to be used by the child process
//
// This is used during fork() when no alternate stack is provided
static inline int copy_stack(struct process *parent_proc, struct process *proc)
{
	int error = 0;
	uintptr_t heap_start;
	uintptr_t stack_size;

	heap_start = parent_proc->map->heap_start;
	stack_size = parent_proc->map->stack_end - parent_proc->map->heap_start;

	error = memory_map_unmap(proc->map, parent_proc->map->heap_start, parent_proc->map->stack_end - parent_proc->map->heap_start);
	if (error < 0)
		return error;

	error = memory_map_insert_heap_stack(proc->map, heap_start, stack_size);
	if (error < 0)
		return error;

	error = memory_map_move_sbrk(proc->map, parent_proc->map->sbrk - parent_proc->map->heap_start);
	if (error < 0)
		return error;

	memcpy((char *) proc->map->heap_start, (char *) parent_proc->map->heap_start, stack_size);

	// Adjust the pointers to the command line arguments to point to the new stack
	proc->map->argv = (const char *const *) adjust_string_array((char **) parent_proc->map->argv, (char *) parent_proc->map->heap_start, (char *) proc->map->heap_start);
	proc->map->envp = (const char *const *) adjust_string_array((char **) parent_proc->map->envp, (char *) parent_proc->map->heap_start, (char *) proc->map->heap_start);

	return 0;
}

char **adjust_string_array(char **source, char *parent_stack_start, char *new_stack_start)
{
	short i;
	char **dest;

	dest = (char **) (((char *) source) - parent_stack_start + new_stack_start);

	for (i = 0; source[i]; i++) {
		dest[i] = source[i] - parent_stack_start + new_stack_start;
	}
	dest[i] = NULL;

	return dest;
}
#endif

