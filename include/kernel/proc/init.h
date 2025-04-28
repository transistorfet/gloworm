
#ifndef _KERNEL_PROC_INIT_H
#define _KERNEL_PROC_INIT_H

struct process;

struct process *create_init_task();
struct process *create_kernel_task(const char *name, int (*task_start)());
int idle_task();

#endif
