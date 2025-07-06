
#ifndef _KERNEL_ARCH_CONTEXT_H
#define _KERNEL_ARCH_CONTEXT_H

#include <stdint.h>

struct process;

/// Architecture-specific state for a task (process/thread)
struct arch_task_info;

void *arch_get_user_stackp(struct process *proc);
void arch_set_user_stackp(struct process *proc, void *usp);
void *arch_get_kernel_stackp(struct process *proc);
void arch_set_kernel_stackp(struct process *proc, void *ksp);

int arch_init_task_info(struct process *proc);
int arch_release_task_info(struct process *proc);
int arch_add_process_context(struct process *proc, char *user_sp, void *entry);
int arch_clone_task_info(struct process *parent_proc, struct process *proc, char *user_sp);
int arch_add_signal_context(struct process *proc, int signum);
int arch_remove_signal_context(struct process *proc);

extern void *create_context(void *kernel_stack, void *entry, void *user_stack);
extern void *drop_context(void *kernel_stack);
extern void _exit();
extern void _sigreturn();

#endif
