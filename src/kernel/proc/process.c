
#include <stddef.h>

#include <asm/irqs.h>
#include <kernel/api.h>
#include <kernel/mm/map.h>
#include <kernel/arch/context.h>
#include <kernel/proc/timer.h>
#include <kernel/proc/process.h>
#include <kernel/proc/filedesc.h>
#include <kernel/proc/scheduler.h>
#include <kernel/utils/queue.h>
#include <kernel/utils/macros.h>



// Process Table and Queues
#define MAX_PROCESSES		CONFIG_MAX_PROCESSES
static pid_t next_pid;
static int num_processes;
static int next_process_slot;
static struct process *process_list[CONFIG_MAX_PROCESSES];

extern struct process *current_proc;

static int next_slotnum(void);

void init_proc(void)
{
	next_pid = 2;
	num_processes = 0;
	next_process_slot = 0;

	for (int i = 0; i < CONFIG_MAX_PROCESSES; i++) {
		process_list[i] = NULL;
	}

	extern struct process *primordial_process;
	primordial_process = NULL;
}

struct process *new_proc(pid_t pid, uid_t uid)
{
	short slotnum;
	short saved_status;
	struct process *proc;

	if (!pid)
		pid = next_pid++;

	if (num_processes >= MAX_PROCESSES) {
		return NULL;
	}

	slotnum = next_slotnum();
	if (slotnum < 0) {
		return NULL;
	}

	proc = kzalloc(sizeof(struct process));
	if (!proc) {
		return NULL;
	}

	LOCK(saved_status);

	proc->slotnum = slotnum;
	process_list[proc->slotnum] = proc;
	num_processes += 1;

	_queue_node_init(&proc->run_node);

	proc->tid = pid;
	proc->tgid = pid;
	proc->pid = pid;
	proc->parent = INIT_PID;
	proc->pgid = proc->pid;
	proc->session = proc->pid;
	proc->ctty = 0;

	proc->state = PS_RUNNING;
	proc->map = NULL;
	init_timer(&proc->timer);
	init_signal_data(proc);

	proc->start_time = get_system_time();

	proc->uid = uid;
	proc->umask = PROC_DEFAULT_UMASK;
	proc->fd_table = NULL;

	arch_init_task_info(proc);
	insert_proc(proc);

	UNLOCK(saved_status);
	return proc;
}

struct process *get_proc(pid_t pid)
{
	short saved_status;

	LOCK(saved_status);
	for (int i = 0; i < CONFIG_MAX_PROCESSES; i++) {
		if (process_list[i] && process_list[i]->pid == pid) {
			UNLOCK(saved_status);
			return process_list[i];
		}
	}
	UNLOCK(saved_status);
	return NULL;
}

int reset_proc(struct process *proc)
{
	proc->wait_events = 0;
	proc->wait_check = NULL;
	memset(&proc->blocked_call, 0, sizeof(struct syscall_record));
	init_timer(&proc->timer);
	init_signal_data(proc);

	if (proc->fd_table) {
		struct fd_table *fd_table;

		fd_table = alloc_fd_table();
		if (!fd_table) {
			return ENOMEM;
		}
		dup_fd_table(fd_table, proc->fd_table, 2);
		free_fd_table(proc->fd_table);
		proc->fd_table = fd_table;
	}

	if (proc->map) {
		memory_map_free(proc->map);
		proc->map = NULL;
	}

	if (previous_proc == proc) {
		previous_proc = NULL;
	}

	return 0;
}

int close_proc(struct process *proc)
{
	short orphans = 0;
	short saved_status;

	// Set the previous process to NULL so that we skip over attempting to
	// save the context during restore_context
	if (proc == previous_proc) {
		previous_proc = NULL;
	}

	remove_timer(&proc->timer);
	if (proc->fd_table) {
		free_fd_table(proc->fd_table);
	}
	if (proc->map) {
		memory_map_free(proc->map);
		proc->map = NULL;
	}

	LOCK(saved_status);
	// Reassign any child procs' parent to be 1 (init), since we can't be sure this proc's
	// parent is waiting, and the zombie proc wont get recycled
	for (int i = 0; i < CONFIG_MAX_PROCESSES; i++) {
		if (process_list[i] && process_list[i]->parent == proc->pid) {
			orphans += 1;
			process_list[i]->parent = INIT_PID;
		}
	}
	UNLOCK(saved_status);

	arch_release_task_info(proc);

	return orphans;
}


void cleanup_proc(struct process *proc)
{
	short saved_status;

	LOCK(saved_status);
	if (process_list[proc->slotnum] == proc) {
		process_list[proc->slotnum] = NULL;
		num_processes -= 1;
	}
	UNLOCK(saved_status);
	kmfree(proc);
}

struct process *find_exited_child(pid_t parent, pid_t child)
{
	for (int i = 0; i < CONFIG_MAX_PROCESSES; i++) {
		if (process_list[i] && process_list[i]->state == PS_EXITED && process_list[i]->parent == parent && (child == -1 || process_list[i]->pid == child)) {
			return process_list[i];
		}
	}
	return NULL;
}

static int on_alarm(struct timer *timer, struct process *proc)
{
	proc->bits &= ~PB_ALARM_ON;
	send_signal(proc->pid, SIGALRM);
	return 0;
}

int set_proc_alarm(struct process *proc, uint32_t seconds)
{
	if (!seconds) {
		remove_timer(&proc->timer);
	} else {
		proc->bits |= PB_ALARM_ON;
		proc->timer.argp = proc;
		proc->timer.callback = (timer_callback_t) on_alarm;
		add_timer(&proc->timer, seconds, 0);
	}
	return 0;
}


void proc_iter_start(struct process_iter *iter)
{
	iter->slotnum = 0;
}

struct process *proc_iter_next(struct process_iter *iter)
{
	struct process *proc;

	do {
	        if (iter->slotnum >= CONFIG_MAX_PROCESSES)
	                return NULL;
	        proc = process_list[iter->slotnum];
		iter->slotnum += 1;
	} while (!proc);
	return proc;
}

static int next_slotnum(void)
{
	char once = 0;

	while (process_list[next_process_slot]) {
		next_process_slot += 1;
		if (next_process_slot > CONFIG_MAX_PROCESSES) {
			if (once) {
				return -1;
			}
			next_process_slot = 0;
			once = 1;
		}
	}
	return next_process_slot++;
}

