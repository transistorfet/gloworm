
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

char **adjust_string_array(char **source, char *parent_stack_start, char *new_stack_start);
static inline int remap_all_copy_on_write(struct process *parent_proc, struct process *proc, struct memory_map *new_map);
static inline int copy_stack(struct process *parent_proc, struct process *proc, struct memory_map *new_map);
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
		log_debug("failed to clone process %d from %d: %d\n", proc->pid, parent_proc->pid, error);
		exit_proc(proc, error);
		cleanup_proc(proc);
		return error;
	}

	if (args->stack) {
		// Put the argument onto the stack before initializing the context
		args->stack -= sizeof(void *);
		*((void **) args->stack) = args->arg;
		arch_add_process_context(proc, args->stack, args->entry);
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
		// TODO enable copy on write for both the new map, and the parent map
		error = memory_map_mmap(new_map, cur->start, cur->end - cur->start, cur->flags, MEMORY_OBJECT_MAKE_REF(MEMORY_OBJECT_ACCESS(cur)), cur->offset);
		if (error < 0)
			goto fail;
		#if defined(CONFIG_MMU)
		// TODO remove this later
		// Inecrement the refcount on each page in the new memory area
		// TODO also probably needs a better check, for SEG_WINDOW or something, or just use the page table addresses to determine?
		// it might have to check multiple page blocks, but it's a fork, so it's maybe not as big of a deal?
		if (cur->start >= 0x200000) {
			page_make_ref_contiguous((page_t *) cur->start, cur->end - cur->start);
		}
		#endif
	}

	#if defined(CONFIG_MMU)

	error = remap_all_copy_on_write(parent_proc, proc, new_map);
	if (error < 0)
		goto fail;

	// TODO remove later
	error = copy_stack(parent_proc, proc, new_map);
	if (error < 0)
		goto fail;

	#else

	error = copy_stack(parent_proc, proc, new_map);
	if (error < 0)
		goto fail;

	#endif

	proc->map = new_map;

	char *stack_pointer = NULL;
	// TODO this needs to move to a common location, but the reason it can't (hasn't been) moved is because it needs the stack pointer?
	// because it's adjusting both the user and kernel stacks, due to the fact that enabling user mode uses two stacks instead of one
	stack_pointer = (char *) new_map->stack_end - (parent_proc->map->stack_end - (uintptr_t) arch_get_user_stackp(parent_proc));
	if (!stack_pointer)
		return EFAULT;
	arch_clone_task_info(parent_proc, proc, stack_pointer);

	return 0;

fail:
	if (new_map)
		memory_map_free(new_map);
	return error;
}

static inline int remap_all_copy_on_write(struct process *parent_proc, struct process *proc, struct memory_map *new_map)
{
	int error = 0;
	struct memory_segment *cur;

	// Remap all in parent process
	for (cur = memory_map_iter_first(parent_proc->map); cur; cur = memory_map_iter_next(cur)) {
		error = memory_map_remap_copy_on_write(parent_proc->map, cur->start, cur->end - cur->start);
		if (error < 0)
			return error;
	}

	// Remap all in child process
	for (cur = memory_map_iter_first(new_map); cur; cur = memory_map_iter_next(cur)) {
		error = memory_map_remap_copy_on_write(new_map, cur->start, cur->end - cur->start);
		if (error < 0)
			return error;
	}

	return 0;
}

static inline int copy_stack(struct process *parent_proc, struct process *proc, struct memory_map *new_map)
{
	int error = 0;
	uintptr_t heap_start;
	uintptr_t stack_size;

	heap_start = parent_proc->map->heap_start;
	stack_size = parent_proc->map->stack_end - parent_proc->map->heap_start;

	#if !defined(CONFIG_MMU)
	error = memory_map_unmap(new_map, parent_proc->map->heap_start, parent_proc->map->stack_end - parent_proc->map->heap_start);
	if (error < 0)
		return error;
	#endif

	error = memory_map_insert_heap_stack(new_map, heap_start, stack_size);
	if (error < 0)
		return error;

	error = memory_map_move_sbrk(new_map, parent_proc->map->sbrk - parent_proc->map->heap_start);
	if (error < 0)
		return error;

	memcpy((char *) new_map->heap_start, (char *) parent_proc->map->heap_start, stack_size);

	// Adjust the pointers to the command line arguments to point to the new stack
	new_map->argv = (const char *const *) adjust_string_array((char **) parent_proc->map->argv, (char *) parent_proc->map->heap_start, (char *) new_map->heap_start);
	new_map->envp = (const char *const *) adjust_string_array((char **) parent_proc->map->envp, (char *) parent_proc->map->heap_start, (char *) new_map->heap_start);

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

