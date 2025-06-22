
#ifndef _KERNEL_PROC_EXEC_H
#define _KERNEL_PROC_EXEC_H

struct process;
struct memory_map;

void *exec_initialize_stack_entry(struct memory_map *map, void *stack_pointer, void *entry);
void *exec_initialize_stack_with_args(struct memory_map *map, void *stack_pointer, void *entry, const char *const argv[], const char *const envp[]);
int exec_reset_and_initialize_stack(struct process *proc, struct memory_map *map, void *entry, const char *const argv[], const char *const envp[]);
int exec_alloc_new_stack(struct process *proc, struct memory_map *map, int stack_size, void *entry, const char *const argv[], const char *const envp[]);

#endif

