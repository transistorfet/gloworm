
#ifndef _KERNEL_PROC_FORK_H
#define _KERNEL_PROC_FORK_H

#include <sys/types.h>

struct process;

struct clone_args {
	int flags;
	int (*entry)(void *);
	void *stack;
	void *arg;
};

int clone_process(struct process *parent_proc, struct clone_args *args, struct process **proc);
int clone_process_memory(struct process *parent_proc, struct process *proc, int flags);

#endif

