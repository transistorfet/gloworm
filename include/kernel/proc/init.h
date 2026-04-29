
#ifndef _KERNEL_PROC_INIT_H
#define _KERNEL_PROC_INIT_H

struct process;

struct process *create_idle_thread(void);
struct process *clone_kernel_thread(int (*thread_start)(), const char *const argv[], const char *const envp[]);
struct process *create_init_task(void);

#endif
