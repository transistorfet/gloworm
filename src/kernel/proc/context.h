
#ifndef _SRC_KERNEL_PROC_CONTEXT_H
#define _SRC_KERNEL_PROC_CONTEXT_H

#include <stdint.h>

struct process;

extern void *create_context(void *user_stack, void *entry, void *exit);
extern void *drop_context(void *user_stack);
extern void set_context_return_value(struct process *proc, uint32_t value);
extern void _exit();
extern void _sigreturn();

#endif
