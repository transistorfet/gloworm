
#ifndef _KERNEL_PROCESS_H
#define _KERNEL_PROCESS_H

#include <stddef.h>
#include <kernel/syscall.h>
#include <kernel/proc/timer.h>
#include <kernel/proc/signal.h>
#include <kernel/proc/filedesc.h>
#include <kernel/proc/memory.h>
#include <kernel/proc/scheduler.h>
#include <kernel/utils/queue.h>

#define INIT_PID			1

#define PB_BLOCK_CONDITIONS		0x00FF
#define PB_SYSCALL			0x0001
#define PB_WAITING			0x0002
#define PB_PAUSED			0x0004
#define PB_SELECT			0x0008
#define PB_ALARM_ON			0x0100
#define PB_DONT_SET_RETURN_VAL		0x0200

#define PROC_IS_RUNNING(proc)		((proc)->state == PS_RUNNING || (proc)->state == PS_RESUMING)

typedef enum {
	PS_RUNNING,
	PS_RESUMING,
	PS_BLOCKED,
	PS_STOPPED,
	PS_EXITED,
} proc_state_t;


#define PROC_DEFAULT_UMASK	022

struct vnode;
struct process;

struct process {
	struct queue_node node;
	/// The kernel stack pointer for this process
	/// If user mode is not used, this will also be the user stack pointer
	void *sp;
	uintptr_t return_value;
	uint16_t state;

	pid_t tid;
	pid_t tgid;
	pid_t pid;
	pid_t parent;
	pid_t pgid;
	pid_t session;

	struct memory_map *map;

	//#if defined(CONFIG_USER_MODE)
	///// A pointer to the start of the kernel stack for this process
	//page_t kernel_stack;
	///// The size of the allocated kernel stack
	//ssize_t kernel_stack_size;
	//#endif

	// TODO somewhere in here, or in the context, would be:
	// page_t kernel_stack;
	// mmu_table_t *root_table;
	// uint32_t usp;

	uint32_t bits;
	int exitcode;
	int wait_events;
	wait_check_t wait_check;
	struct syscall_record blocked_call;
	struct timer timer;
	struct signal_data signals;

	time_t start_time;
	clock_t user_time;
	clock_t sys_time;

	uid_t uid;
	mode_t umask;
	device_t ctty;
	struct vnode *cwd;
	struct fd_table *fd_table;
};

struct process_iter {
	short slot;
};

int init_proc();
struct process *new_proc(pid_t pid, uid_t uid);
struct process *get_proc(pid_t pid);
void close_proc(struct process *proc);
void cleanup_proc(struct process *proc);
struct process *find_exited_child(pid_t parent, pid_t child);
int set_proc_alarm(struct process *proc, uint32_t seconds);

void proc_iter_start(struct process_iter *iter);
struct process *proc_iter_next(struct process_iter *iter);

#endif
