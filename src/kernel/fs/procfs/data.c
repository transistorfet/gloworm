
#include <stdio.h>
#include <string.h>
#include <sys/param.h>

#include <kernel/proc/memory.h>
#include <kernel/proc/process.h>
#include <kernel/utils/usercopy.h>
#include <kernel/utils/iovec.h>

#include "data.h"


static inline char get_proc_state(struct process *proc);
static inline size_t get_proc_size(struct process *proc);


int get_data_cmdline(struct process *proc, char *buffer, int max)
{
	int i = 0;
	int result;
	int length;

	for (const char *const *arg = proc->map->argv; ; arg++) {
		#if defined(CONFIG_MMU)

		char *argstr;
		struct kvec kvec[10];

		result = memory_map_load_pages_into_kvec(proc->map, kvec, 10, (virtual_address_t) arg, sizeof(uintptr_t), 0);
		if (result < 0) {
			return result;
		}
		result = memcpy_out_of_kvec(kvec, result, 0, &argstr, sizeof(uintptr_t));
		if (result < 0) {
			return result;
		}

		if (!argstr) {
			break;
		}

		// TODO this is a hacky way of avoiding requesting pages beyond the end of the stack
		length = roundup((uintptr_t) argstr, PAGE_SIZE) - (uintptr_t) argstr;
		result = memory_map_load_pages_into_kvec(proc->map, kvec, 10, (virtual_address_t) argstr, length, 0);
		if (result < 0) {
			return result;
		}
		result = strncpy_out_of_kvec(kvec, result, 0, &buffer[i], max - i);
		if (result < 0) {
			return result;
		}

		#else

		if (!arg) {
			break;
		}

		strncpy(&buffer[i], (const char *) arg, max - i);
		result = strnlen(&buffer[i], max - i);

		#endif

		i += result;
		buffer[i] = ' ';
		if (i > max) {
			break;
		}
	}
	buffer[i] = '\0';
	return i;
}


int get_data_stat(struct process *proc, char *buffer, int max)
{
	const char *cmd;

	#if defined(CONFIG_MMU)

	int result;
	int length;
	uintptr_t arg;
	struct kvec kvec[10];
	char cmd_buffer[32];

	if (proc->map->argv) {
		result = memory_map_load_pages_into_kvec(proc->map, kvec, 10, (virtual_address_t) proc->map->argv, sizeof(uintptr_t), 0);
		if (result < 0) {
			return result;
		}
		result = memcpy_out_of_kvec(kvec, result, 0, &arg, sizeof(uintptr_t));
		if (result < 0) {
			return result;
		}

		// TODO this is a hacky way of avoiding requesting pages beyond the end of the stack
		length = roundup(arg, PAGE_SIZE) - arg;
		result = memory_map_load_pages_into_kvec(proc->map, kvec, 10, (virtual_address_t) arg, length, 0);
		if (result < 0) {
			return result;
		}
		result = strncpy_out_of_kvec(kvec, result, 0, cmd_buffer, 32);
		if (result < 0) {
			return result;
		}
		cmd = cmd_buffer;
	} else {
		cmd_buffer[0] = '\0';
		cmd = cmd_buffer;
	}
	#else

	cmd = proc->map->argv[0];

	#endif

	return snprintf(buffer, max,
		"%d %s %c %d %d %d %d %ld %ld\n",
		proc->pid,
		cmd,
		get_proc_state(proc),
		proc->parent,
		proc->pgid,
		proc->session,
		proc->ctty,
		proc->start_time,
		get_proc_size(proc)
	);
}

int get_data_statm(struct process *proc, char *buffer, int max)
{
	return snprintf(buffer, max,
		"%ld %lx %lx %lx %lx %lx %lx\n",
		get_proc_size(proc),
		(uintptr_t) proc->map->code_start,
		proc->map->data_start - proc->map->code_start,
		(uintptr_t) proc->map->data_start,
		proc->map->sbrk - proc->map->data_start,
		(uintptr_t) proc->map->sbrk,
		proc->map->stack_end
	);

}

int get_data_maps(struct process *proc, char *buffer, int max)
{
	int i = 0;

	for (struct memory_segment *cur = memory_map_iter_first(proc->map); cur; cur = memory_map_iter_next(cur)) {
		i += snprintf(&buffer[i], max - i, "%lx-%lx %x\n", cur->start, cur->end, cur->flags);
	}
	return i;
}


static inline char get_proc_state(struct process *proc)
{
	switch (proc->state) {
		case PS_RESUMING:
		case PS_RUNNING:
			return 'R';
		case PS_BLOCKED:
			return 'S';
		case PS_STOPPED:
			return 'D';
		case PS_EXITED:
			return 'Z';
		default:
			return '?';
	}
}

static inline size_t get_proc_size(struct process *proc)
{
	size_t size = 0;

	for (struct memory_segment *cur = memory_map_iter_first(proc->map); cur; cur = memory_map_iter_next(cur)) {
		size += cur->end - cur->start;
	}
	return size;
}

int get_data_mounts(struct process *proc, char *buffer, int max)
{
	size_t i = 0;
	char name[100];
	struct mount *mp;
	struct mount_iter iter;
	extern struct process *current_proc;

	vfs_mount_iter_start(&iter);

	while ((mp = vfs_mount_iter_next(&iter))) {
		vfs_reverse_lookup(mp->mount_node, name, 100, current_proc->uid);
		i += snprintf(&buffer[i], max - i, "%s %x %s %s\n", name, mp->dev, mp->ops->fstype, (mp->bits & VFS_MBF_READ_ONLY) ? "ro" : "rw");
	}
	return i;
}

