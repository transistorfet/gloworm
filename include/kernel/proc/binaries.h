
#ifndef _KERNEL_PROC_BINARIES_H
#define _KERNEL_PROC_BINARIES_H

#include <kernel/utils/strarray.h>

struct process;

int load_binary(const char *path, struct process *proc, struct string_array *argv, struct string_array *envp);

#endif
