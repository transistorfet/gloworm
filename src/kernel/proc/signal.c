
#include <errno.h>

#include <sys/types.h>
#include <kernel/signal.h>
#include <kernel/scheduler.h>

#include "process.h"
#include "context.h"


#define SIG_MASK_DEFAULT_IGNORE		0x00010000
#define SIG_MASK_DEFAULT_TERMINATE	0x47807FFF
#define SIG_MASK_DEFAULT_STOP		0x003C0000
#define SIG_MASK_MASKABLE		0xFFFBFEFF


struct sigcontext {
	int signum;
	sigset_t prev_mask;
};

static inline void run_signal_handler(struct process *proc, int signum);
static inline void run_signal_default_action(struct process *proc, int signum, sigset_t sigmask);
static inline sigset_t signal_to_map(int signum);
static inline int get_next_signum(sigset_t mask);

void init_signal_data(struct process *proc)
{
	proc->signals.ignored = 0;
	proc->signals.blocked = 0;
	proc->signals.pending = 0;

	for (short i = 0; i < SIG_HANDLERS_NUM; i++)
		proc->signals.actions[i].sa_handler = NULL;
}

int get_signal_action(struct process *proc, int signum, struct sigaction *act)
{
	*act = proc->signals.actions[signum - 1];
	return 0;
}

int set_signal_action(struct process *proc, int signum, const struct sigaction *act)
{
	proc->signals.actions[signum - 1] = *act;
	proc->signals.actions[signum - 1].sa_mask |= signal_to_map(signum);
	proc->signals.actions[signum - 1].sa_mask &= SIG_MASK_MASKABLE;
	return 0;
}

int send_signal(pid_t pid, int signum)
{
	struct process *proc;

	proc = get_proc(pid);
	if (!proc)
		return ESRCH;
	return dispatch_signal(proc, signum);
}

int send_signal_process_group(pid_t pgid, int signum)
{
	struct process *proc;
	struct process_iter iter;

	proc_iter_start(&iter);
	while ((proc = proc_iter_next(&iter))) {
		if (proc->pgid == pgid)
			dispatch_signal(proc, signum);
	}
	return 0;
}

int dispatch_signal(struct process *proc, int signum)
{
	sigset_t sigmask;

	if (signum <= 0 || signum >= 32)
		return EINVAL;
	sigmask = signal_to_map(signum);

	if (sigmask & proc->signals.ignored)
		return 0;

	if (sigmask & proc->signals.blocked) {
		proc->signals.pending |= sigmask;
		return 0;
	}

	if (proc->signals.actions[signum - 1].sa_handler) {
		run_signal_handler(proc, signum);
	} else {
		run_signal_default_action(proc, signum, sigmask);
	}

	return 0;
}

static inline void run_signal_handler(struct process *proc, int signum)
{
	struct sigcontext *context;

	// If the syscall won't be restarted after the handler has run, then cancel it now
	if ((proc->bits & PB_PAUSED) || !(proc->signals.actions[signum - 1].sa_flags & SA_RESTART))
		cancel_syscall(proc);

	// Save signal data on the stack for use by sigreturn
	context = (((struct sigcontext *) proc->sp) - 1);
	context->signum = signum;
	context->prev_mask = proc->signals.blocked;
	proc->signals.blocked |= proc->signals.actions[signum - 1].sa_mask;

	// Push a fresh context onto the stack, which will run the handler and then call sigreturn()
	proc->sp = (void *) context;
	proc->sp = create_context(proc->sp, proc->signals.actions[signum - 1].sa_handler, _sigreturn);

	// Resume the process without restarting the last syscall
	resume_proc_without_restart(proc);
}

void cleanup_signal_handler()
{
	struct sigcontext *context;

	current_proc->sp = drop_context(current_proc->sp);
	context = (struct sigcontext *) current_proc->sp;
	current_proc->signals.blocked = context->prev_mask;
	current_proc->sp = (((struct sigcontext *) current_proc->sp) + 1);

	// TODO maybe we should restart the syscall anyways, which would then block again if it's not ready, instead of suspending here?
	if (current_proc->bits & PB_SYSCALL)
		suspend_proc(current_proc, 0);

	check_pending_signals();
}

void check_pending_signals()
{
	int signum;

	if (current_proc->signals.pending & ~current_proc->signals.blocked) {
		signum = get_next_signum(current_proc->signals.pending);
		if (signum < 0)
			return;
		current_proc->signals.pending &= ~signal_to_map(signum);
		dispatch_signal(current_proc, signum);
	}
}

static inline void run_signal_default_action(struct process *proc, int signum, sigset_t sigmask)
{
	if (sigmask & SIG_MASK_DEFAULT_TERMINATE) {
		exit_proc(proc, -1);
		resume_waiting_parent(proc);
	} else if (sigmask & SIG_MASK_DEFAULT_STOP) {
		stop_proc(proc);
	} else if (signum == SIGCONT) {
		resume_proc(proc);
	}

	// Since we don't execute the signal handler cleanup for a default action, we cancel the syscall here instead
	if (!(current_proc->signals.actions[signum - 1].sa_flags & SA_RESTART))
		cancel_syscall(current_proc);
}

static inline sigset_t signal_to_map(int signum)
{
	return 0x00000001 << (signum - 1);
}

static inline int get_next_signum(sigset_t mask)
{
	for (short i = 1; i < 31; i++) {
		if (mask & 0x01)
			return i;
		mask >>= 1;
	}
	return -1;
}


