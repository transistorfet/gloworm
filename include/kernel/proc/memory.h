
#ifndef _KERNEL_PROC_MEMORY_H
#define _KERNEL_PROC_MEMORY_H

#include <stddef.h>

struct process;

int create_process_memory(struct process *proc, size_t text_size);
void free_process_memory(struct process *proc);
int clone_process_memory(struct process *parent_proc, struct process *proc);
int reset_stack(struct process *proc, void *entry, char *const argv[], char *const envp[]);
int increase_data_segment(struct process *proc, int increase);

#endif
