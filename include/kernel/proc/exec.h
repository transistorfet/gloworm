
#ifndef _KERNEL_PROC_EXEC_H
#define _KERNEL_PROC_EXEC_H

struct process;
struct memory_map;

void exec_initialize_stack_with_args(struct process *proc, void *stack_pointer, void *entry, const char *const argv[], const char *const envp[]);

#endif

