
#ifndef _KERNEL_PROC_EXEC_H
#define _KERNEL_PROC_EXEC_H

#include <asm/addresses.h>
#include <kernel/utils/strarray.h>

struct process;
struct memory_map;

void exec_initialize_kernel_stack_with_args(struct process *proc, virtual_address_t stack_pointer, void *entry, struct string_array *argv, struct string_array *envp);
void exec_initialize_user_stack_with_args(struct process *proc, virtual_address_t stack_pointer, void *entry, struct string_array *argv, struct string_array *envp);

#endif

