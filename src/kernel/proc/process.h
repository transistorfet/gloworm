
#ifndef _SRC_KERNEL_PROCESS_H
#define _SRC_KERNEL_PROCESS_H

#include <stddef.h>
#include <kernel/signal.h>
#include <kernel/syscall.h>
#include <kernel/scheduler.h>

#include "timer.h"
#include "filedesc.h"
#include "../misc/queue.h"

#define INIT_PID		1

#define	NUM_SEGMENTS		3

typedef enum {
	M_TEXT,
	M_DATA,
	M_STACK
} proc_seg_t;

struct mem_seg {
	void *base;
	size_t length;
};

struct mem_map {
	struct mem_seg segments[NUM_SEGMENTS];
};


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
#define PROC_CMDLINE_ARGS	4

struct vnode;
struct process;

struct process {
	struct queue_node node;
	void *sp;
	uint16_t state;

	struct mem_map map;

	pid_t pid;
	pid_t parent;
	pid_t pgid;
	pid_t session;

	uint16_t bits;
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
	const char *cmdline[PROC_CMDLINE_ARGS];
	struct vnode *cwd;
	fd_table_t fd_table;
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
