
#ifndef _KERNEL_ARCH_CONTEXT_H
#define _KERNEL_ARCH_CONTEXT_H

#include <stdint.h>

/// Architecture-specific state for a task (process/thread)
struct arch_task_info;

extern void *create_context(void *kernel_stack, void *entry, void *exit, void *user_stack);
extern void *drop_context(void *kernel_stack);
extern void _exit();
extern void _sigreturn();

#endif
