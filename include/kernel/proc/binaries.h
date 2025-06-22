
#ifndef _KERNEL_PROC_BINARIES_H
#define _KERNEL_PROC_BINARIES_H

struct process;

int load_binary(const char *path, struct process *proc, const char *const argv[], const char *const envp[]);

#endif
