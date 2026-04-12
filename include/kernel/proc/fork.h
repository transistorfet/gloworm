
#ifndef _KERNEL_PROC_FORK_H
#define _KERNEL_PROC_FORK_H

#include <sys/types.h>

struct process;

struct clone_args {
	int flags;
	void *stack;
	// NOTE there is no entry point because that will be handled by the user process
	//	The child process will see a return of 0 from the clone syscall
};

int clone_process(struct process *parent_proc, struct clone_args *args, struct process **proc);
int clone_process_memory(struct process *parent_proc, struct process *proc, int flags);

#endif

