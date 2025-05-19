
#ifndef _KERNEL_ARCH_CONTEXT_H
#define _KERNEL_ARCH_CONTEXT_H

#include <stdint.h>

extern void *create_context(void *user_stack, void *entry, void *exit);
extern void *drop_context(void *user_stack);
extern void _exit();
extern void _sigreturn();

#endif
