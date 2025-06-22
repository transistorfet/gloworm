
#ifndef _KERNEL_PROC_INIT_H
#define _KERNEL_PROC_INIT_H

struct process;

struct process *create_idle_task(void);
struct process *create_init_task(void);
struct process *create_kernel_thread(const char *name, int (*task_start)());

#endif
