
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
#include <kernel/utils/strarray.h>


/// The process which is cloned to make either kernel threads or user processes
///
/// It's actually the idle task as well, and the first task that is created upon startup.
/// All other tasks are created through `clone_process_memory`.
struct process *primordial_process = NULL;

int idle_task(void);
int alloc_kernel_stack(struct process *proc, int (*thread_start)(), const char *const argv[], const char *const envp[]);
int launch_user_task_in_kernel(struct process *proc, void (*entry)(), const char **argv, const char **envp);

/// Create the init process
///
/// This is the first user process which is created, and it executes the init binary
/// (either from disk or embedded in the kernel image) which is always running in the
/// background, waiting on other processes to exit and accept their return code.  Any
/// orphaned processes will become the child of the init process.
struct process *create_init_task(void)
{
	int error = 0;
	struct process *proc;

	proc = new_proc(INIT_PID, SU_UID);
	if (!proc)
		goto fail;

	proc->map = memory_map_alloc();
	if (!proc->map)
		goto fail;

	proc->fd_table = alloc_fd_table();
	if (!proc->fd_table)
		goto fail;

	// Set the current proc, or some of the functions that will be called won't do the right thing
	current_proc = proc;

	#if defined(CONFIG_SHELL_IN_KERNEL)

	const char *envp[1] = { NULL };
	const char *argv[2] = { "init", NULL };

	extern void init_task();
	error = launch_user_task_in_kernel(proc, init_task, argv, envp);
	if (error)
		goto fail;

	#else

	const char *envp[1] = { NULL };
	const char *argv[2] = { "/bin/init", NULL };
	struct string_array argv_buffer, envp_buffer;

	string_array_copy(&argv_buffer, argv, FROM_KERNEL);
	string_array_copy(&envp_buffer, envp, FROM_KERNEL);

	error = load_binary(argv[0], proc, &argv_buffer, &envp_buffer);
	if (error)
		goto fail;

	#endif

	return proc;

fail:
	if (proc)
		exit_proc(proc, -1);
	panic("failed to create init task, %d; stopping\n", error);
	while (1) {}
}

#if defined(CONFIG_SHELL_IN_KERNEL)
int launch_user_task_in_kernel(struct process *proc, void (*entry)(), const char **argv, const char **envp)
{
	int error = 0;
	struct string_array argv_buffer, envp_buffer;

	string_array_copy(&argv_buffer, argv, FROM_KERNEL);
	string_array_copy(&envp_buffer, envp, FROM_KERNEL);

	// Add a code segment for this process, which is the entire kernel
	extern int __kernel_start, __kernel_end;
	error = memory_map_mmap(proc->map, (uintptr_t) &__kernel_start, ((uintptr_t) &__kernel_end) - ((uintptr_t) &__kernel_start), SEG_TYPE_CODE | SEG_READ | SEG_WRITE | SEG_EXECUTABLE | SEG_FIXED, NULL, 0);
	if (error < 0)
		return error;

	// Add the heap and stack segments
	error = memory_map_insert_heap_stack(proc->map, (uintptr_t) &__kernel_end, CONFIG_USER_STACK_SIZE);
	if (error < 0)
		return error;

	// Initialize the stack pointer first, so that the check in memory_map_move_sbrk will pass
	exec_initialize_kernel_stack_with_args(proc, (char *) proc->map->stack_end, entry, &argv_buffer, &envp_buffer);
}
#endif

/// Create the idle thread
///
/// The idle thread is the first task created, and also the primordial task that all other
/// kernel threads and user processes are cloned from.`
struct process *create_idle_thread(void)
{
	int error = ENOMEM;
	struct process *proc;
	const char *argv[2] = { "[idle]", NULL }, *envp[1] = { NULL };

	if (primordial_process) {
		panic("attempted to create a second primordial process\n");
	}

	proc = new_proc(0, SU_UID);
	if (!proc)
		goto fail;

	extern struct memory_map *kernel_memory_map;
	proc->map = kernel_memory_map;

	proc->fd_table = alloc_fd_table();
	if (!proc->fd_table)
		goto fail;

	primordial_process = proc;

	error = alloc_kernel_stack(proc, idle_task, argv, envp);
	if (error < 0)
		goto fail;

	return proc;

fail:
	if (proc)
		exit_proc(proc, -1);
	panic("failed to create primordial process, %d; stopping\n", error);
	while (1) {}
}

/// Create a kernel thread
///
/// A kernel thread uses the same address space and file table as the kernel
/// but has its own stack and takes a starting address to execute.
/// Currently the only kernel thread is the idle task.
struct process *clone_kernel_thread(int (*thread_start)(), const char *const argv[], const char *const envp[])
{
	int error = ENOMEM;
	struct process *proc;
	struct clone_args args;

	args.flags = CLONE_THREAD | CLONE_VM | CLONE_FS | CLONE_FILES;
	args.entry = NULL;
	args.stack = NULL;
	args.arg = NULL;

	error = clone_process(primordial_process, &args, &proc);
	if (error < 0)
		goto fail;

	error = alloc_kernel_stack(proc, thread_start, argv, envp);
	if (error < 0)
		goto fail;

	return proc;

fail:
	if (proc)
		exit_proc(proc, -1);
	log_error("error when creating kernel task: %d\n", error);
	return NULL;
}

int alloc_kernel_stack(struct process *proc, int (*thread_start)(), const char *const argv[], const char *const envp[])
{
	char *stack;

	// Allocate a new stack, which doesn't have to be mapped, because the kernel map has all of memory mapped 1:1
	stack = (char *) page_alloc_contiguous(PAGE_SIZE);
	proc->map->sbrk = (uintptr_t) stack;
	proc->map->stack_end = (uintptr_t) stack + PAGE_SIZE;

	struct string_array argv_array, envp_array;

	string_array_copy(&argv_array, argv, FROM_KERNEL);
	string_array_copy(&envp_array, envp, FROM_KERNEL);

	exec_initialize_kernel_stack_with_args(proc, proc->map->stack_end, thread_start, &argv_array, &envp_array);

	return 0;
}

int idle_task(void)
{
	// TODO it should be possible to implement this per-architecture to take advantage of sleep/halt instructions
	while (1) {}
}

