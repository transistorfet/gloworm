
#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <sys/sched.h>

#include <asm/context.h>
#include <kernel/printk.h>
#include <kernel/mm/pages.h>
#include <kernel/proc/fork.h>
#include <kernel/proc/exec.h>
#include <kernel/proc/memory.h>
#include <kernel/arch/context.h>
#include <kernel/proc/process.h>


static inline int duplicate_memory_map(struct process *parent_proc, struct process *proc);


int clone_process(struct process *parent_proc, struct clone_args *args, struct process **result)
{
	int error;
	struct process *proc;

	proc = new_proc(0, parent_proc->uid);
	if (!proc) {
		panic("Ran out of procs\n");
	}

	proc->parent = parent_proc->pid;
	proc->pgid = parent_proc->pgid;
	proc->session = parent_proc->session;
	proc->ctty = parent_proc->ctty;

	error = clone_process_memory(parent_proc, proc, args->flags);
	if (error < 0) {
		goto fail;
	}

	error = arch_clone_task_info(parent_proc, proc, args->stack);
	if (error < 0) {
		goto fail;
	}

	// Apply return value to the stack context of the cloned proc, and return to the parent with the new pid
	set_proc_return_value(proc, 0);

	*result = proc;
	return 0;

fail:
	log_debug("failed to clone process %d from %d: %d\n", proc->pid, parent_proc->pid, error);
	exit_proc(proc, error);
	cleanup_proc(proc);
	return error;
}

int clone_process_memory(struct process *parent_proc, struct process *proc, int flags)
{
	int error;

	if (flags & CLONE_THREAD) {
		// The new process has a new pid by default, but we want a new thread instead
		// so set the thread group and pid to match the parent process, leaving the
		// tid to be the only new id in the new process
		proc->tgid = parent_proc->pid;
		proc->pid = parent_proc->pid;
	}

	if (flags & CLONE_VM) {
		proc->map = memory_map_make_ref(parent_proc->map);
	} else {
		// Duplicate the memory map
		error = duplicate_memory_map(parent_proc, proc);
		if (error < 0)
			return error;
	}

	if (flags & CLONE_FILES) {
		proc->fd_table = make_ref_fd_table(parent_proc->fd_table);
	} else {
		if (proc->fd_table) {
			free_fd_table(proc->fd_table);
		}

		proc->fd_table = alloc_fd_table();
		if (!proc->fd_table) {
			return ENOMEM;
		}
		dup_fd_table(proc->fd_table, parent_proc->fd_table, 0);
	}

	// Copy the relevant process data from the parent to child
	if (parent_proc->cwd) {
		proc->cwd = vfs_clone_vnode(parent_proc->cwd);
	} else {
		proc->cwd = NULL;
	}

	return 0;
}

static inline int duplicate_memory_map(struct process *parent_proc, struct process *proc)
{
	int error = 0;
	struct memory_segment *cur;
	struct memory_map *new_map = NULL;

	// Check that the parent has a map and
	// that new proc does *not* have a map
	if (!parent_proc->map || proc->map)
		goto fail;

	new_map = memory_map_alloc();
	if (!new_map)
		return ENOMEM;

	for (cur = memory_map_iter_first(parent_proc->map); cur; cur = memory_map_iter_next(cur)) {
		error = memory_map_copy_segment(new_map, parent_proc->map, cur);
		if (error < 0)
			goto fail;
	}

	new_map->heap_start = parent_proc->map->heap_start;
	new_map->sbrk = parent_proc->map->sbrk;
	new_map->argv = parent_proc->map->argv;
	new_map->envp = parent_proc->map->envp;

	proc->map = new_map;

	return 0;

fail:
	if (new_map)
		memory_map_free(new_map);
	return error;
}

