
#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <sys/sched.h>
#include <kernel/printk.h>
#include <kernel/mm/pages.h>
#include <kernel/proc/fork.h>
#include <kernel/proc/exec.h>
#include <kernel/proc/memory.h>
#include <kernel/arch/context.h>
#include <kernel/proc/process.h>

char **adjust_string_array(char **source, char *parent_stack_start, char *new_stack_start);
static inline int copy_stack(struct process *parent_proc, struct process *proc, struct memory_map *new_map, struct memory_object *parent_object);
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
	if (error < 0)
		return error;

	if (args->stack) {
		// Put the argument onto the stack before initializing the context
		args->stack -= sizeof(void *);
		*((void **) args->stack) = args->arg;
		proc->sp = exec_initialize_stack_entry(proc->map, args->stack, args->entry);
	}

	// Apply return value to the stack context of the cloned proc, and return to the parent with the new pid
	set_proc_return_value(proc, 0);

	*result = proc;
	return 0;
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
		proc->fd_table = alloc_fd_table();
		if (!proc->fd_table)
			return ENOMEM;
		dup_fd_table(proc->fd_table, parent_proc->fd_table);
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
	struct memory_area *cur;
	struct memory_map *new_map = NULL;
	struct memory_object *parent_object;

	// Check that the parent has a map and
	// that new proc does *not* have a map
	if (!parent_proc->map || proc->map)
		goto fail;

	new_map = memory_map_alloc();
	if (!new_map)
		return ENOMEM;

	parent_object = NULL;
	for (cur = memory_map_iter_first(parent_proc->map); cur; cur = memory_map_iter_next(cur)) {
		if ((cur->flags & AREA_TYPE) == AREA_TYPE_STACK || (cur->flags & AREA_TYPE) == AREA_TYPE_HEAP) {
			// Save the last object (which will be the stack) but don't copy
			// the heap or stack segments so we can insert new ones later
			parent_object = cur->object;
		} else {
			error = memory_map_mmap(new_map, cur->start, cur->end - cur->start, cur->flags, memory_object_make_ref(cur->object));
			if (error < 0)
				goto fail;
		}
	}

	if (!parent_object) {
		error = EFAULT;
		goto fail;
	}

	error = copy_stack(parent_proc, proc, new_map, parent_object);
	if (error < 0)
		goto fail;

	return 0;

fail:
	if (new_map)
		memory_map_free(new_map);
	return error;
}

static inline int copy_stack(struct process *parent_proc, struct process *proc, struct memory_map *new_map, struct memory_object *parent_object)
{
	int error = 0;
	int stack_size;
	char *stack_pointer = NULL;

	stack_size = parent_proc->map->stack_end - parent_proc->map->heap_start;

	error = memory_map_insert_heap_stack(new_map, stack_size);
	if (error < 0)
		return error;

	stack_pointer = (char *) new_map->stack_end - (parent_proc->map->stack_end - (uintptr_t) parent_proc->sp);
	if (!stack_pointer)
		return EFAULT;

	error = memory_map_move_sbrk(new_map, parent_proc->map->sbrk - parent_proc->map->heap_start);
	if (error < 0)
		return error;

	memcpy((char *) new_map->heap_start, (char *) parent_proc->map->heap_start, stack_size);

	// Adjust the pointers to the command line arguments to point to the new stack
	new_map->argv = (const char *const *) adjust_string_array((char **) parent_proc->map->argv, (char *) parent_proc->map->heap_start, (char *) new_map->heap_start);
	new_map->envp = (const char *const *) adjust_string_array((char **) parent_proc->map->envp, (char *) parent_proc->map->heap_start, (char *) new_map->heap_start);

	proc->map = new_map;
	proc->sp = stack_pointer;

	return 0;
}

char **adjust_string_array(char **source, char *parent_stack_start, char *new_stack_start)
{
	short i;
	char **dest;

	dest = (char **) (((char *) source) - parent_stack_start + new_stack_start);

	for (i = 0; source[i]; i++) {
		dest[i] = source[i] - parent_stack_start + new_stack_start;
	}
	dest[i] = NULL;

	return dest;
}

