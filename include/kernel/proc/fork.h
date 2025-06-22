
#ifndef _KERNEL_PROC_FORK_H
#define _KERNEL_PROC_FORK_H

struct process;

int clone_process_memory(struct process *parent_proc, struct process *proc, int flags);

#endif

