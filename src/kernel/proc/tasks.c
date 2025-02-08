
#include <stddef.h>
#include <kernel/kmalloc.h>
#include <kernel/printk.h>

#include "../api.h"
#include "../interrupts.h"

#include "memory.h"
#include "process.h"
#include "context.h"
#include "binaries.h"

struct process *create_init_task()
{
	int error;
	char *argv[2] = { "init", NULL }, *envp[1] = { NULL };

	struct process *proc = new_proc(INIT_PID, SU_UID);
	if (!proc)
		panic("Ran out of procs\n");

	// Setup memory segments
	int stack_size = 0x2000;
	proc->map.segments[M_DATA].base = kmalloc(stack_size);
	proc->map.segments[M_DATA].length = 0;
	proc->map.segments[M_STACK].base = proc->map.segments[M_DATA].base;
	proc->map.segments[M_STACK].length = stack_size;

	#if !defined(CONFIG_SHELL_IN_KERNEL)

	// TODO this would load and execute an actual binary from the mounted disk
	void *entry;
	error = load_binary("/bin/init", proc, &entry);
	if (error)
		return NULL;
	reset_stack(proc, entry, argv, envp);

	#else

	proc->map.segments[M_TEXT].base = NULL;
	proc->map.segments[M_TEXT].length = 0x10000;

	extern void init_task();
	reset_stack(proc, init_task, argv, envp);

	#endif

	return proc;
}

struct process *create_kernel_task(const char *name, int (*task_start)())
{
	int error = 0;

	struct process *proc = new_proc(0, SU_UID);
	if (!proc)
		panic("Ran out of procs\n");

	int stack_size = 0x800;
	char *stack = kmalloc(stack_size);
	char *stack_pointer = stack + stack_size;

 	stack_pointer = create_context(stack_pointer, task_start, _exit);

	proc->map.segments[M_TEXT].base = NULL;
	proc->map.segments[M_TEXT].length = 0x10000;
	proc->map.segments[M_DATA].base = kmalloc(stack_size);
	proc->map.segments[M_DATA].length = 0;
	proc->map.segments[M_STACK].base = proc->map.segments[M_DATA].base;
	proc->map.segments[M_STACK].length = stack_size;
	proc->sp = stack_pointer;

	proc->cmdline[0] = name;
	proc->cmdline[1] = NULL;

	return proc;
}

int idle_task()
{
	while (1) { }
}


