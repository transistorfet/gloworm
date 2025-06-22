
#include <stddef.h>
#include <sys/sched.h>

#include <kconfig.h>
#include <asm/irqs.h>
#include <kernel/api.h>
#include <kernel/printk.h>
#include <kernel/arch/context.h>
#include <kernel/mm/pages.h>
#include <kernel/mm/kmalloc.h>
#include <kernel/proc/init.h>
#include <kernel/proc/exec.h>
#include <kernel/proc/fork.h>
#include <kernel/proc/memory.h>
#include <kernel/proc/process.h>
#include <kernel/proc/binaries.h>


/// The process which is cloned to make either kernel threads or user processes
///
/// It's actually the idle task as well, and the first task that is created upon startup.
/// All other tasks are created through `clone_process_memory`.
struct process *primordial_process = NULL;

int idle_task(void);
void alloc_kernel_stack(struct process *proc, int (*task_start)(), const char *const argv[], const char *const envp[]);

/// Create the idle task
///
/// The idle task is the first task created, and also the primordial task that all other
/// kernel threads and user processes are cloned from.`
struct process *create_idle_task(void)
{
	int error = ENOMEM;
	struct process *proc;
	const char *argv[2] = { "idle", NULL }, *envp[1] = { NULL };

	proc = new_proc(0, SU_UID);
	if (!proc)
		goto fail;

	proc->map = memory_map_alloc();
	if (!proc->map)
		goto fail;

	proc->fd_table = alloc_fd_table();
	if (!proc->fd_table)
		goto fail;

	alloc_kernel_stack(proc, idle_task, argv, envp);

	primordial_process = proc;

	return proc;

fail:
	if (proc)
		close_proc(proc);
	panic("failed to create primordial process, %d; stopping", error);
	while (1) {}
}

/// Create the init process
///
/// This is the first user process which is created, and it executes the init binary
/// (either from disk or embedded in the kernel image) which is always running in the
/// background, waiting on other processes to exit and accept their return code.  Any
/// orphaned processes will become the child of the init process.
struct process *create_init_task(void)
{
	int error;
	struct process *proc;
	const char *argv[2] = { "init", NULL }, *envp[1] = { NULL };

	proc = new_proc(INIT_PID, SU_UID);
	if (!proc)
		goto fail;

	proc->map = memory_map_alloc();
	if (!proc->map)
		goto fail;

	proc->fd_table = alloc_fd_table();
	if (!proc->fd_table)
		goto fail;

	#if defined(CONFIG_SHELL_IN_KERNEL)

	extern void init_task();
	error = exec_alloc_new_stack(proc, proc->map, CONFIG_USER_STACK_SIZE, init_task, argv, envp);
	if (error < 0)
		goto fail;

	#else

	void *entry;
	error = load_binary("/bin/init", proc, argv, envp);
	if (error)
		goto fail;

	#endif

	return proc;

fail:
	if (proc)
		close_proc(proc);
	panic("failed to create init task; stopping");
	while (1) {}
}

/// Create a kernel thread
///
/// A kernel thread uses the same address space and file table as the kernel
/// but has its own stack and takes a starting address to execute.
/// Currently the only kernel thread is the idle task.
struct process *create_kernel_thread(const char *name, int (*task_start)())
{
	int error = ENOMEM;
	struct process *proc;
	const char *argv[2] = { name, NULL }, *envp[1] = { NULL };

	proc = new_proc(0, SU_UID);
	if (!proc)
		goto fail;

	error = clone_process_memory(primordial_process, proc, CLONE_VM | CLONE_FS | CLONE_FILES);
	if (error < 0)
		goto fail;

	alloc_kernel_stack(proc, task_start, argv, envp);

	return proc;

fail:
	if (proc)
		close_proc(proc);
	printk_safe("error when creating kernel task: %d\n", error);
	return NULL;
}

void alloc_kernel_stack(struct process *proc, int (*task_start)(), const char *const argv[], const char *const envp[])
{
	char *stack;

	stack = kzalloc(PAGE_SIZE);
	proc->map->sbrk = (uintptr_t) stack;
	proc->sp = exec_initialize_stack_with_args(proc->map, stack + PAGE_SIZE, task_start, argv, envp);
}

int idle_task(void)
{
	// TODO it should be possible to implement this per-architecture to take advantage of sleep/halt instructions
	while (1) {}
}

