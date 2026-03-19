
#include <stdio.h>
#include <string.h>

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
	char *argstr;
	struct kvec kvec[10];
	struct iovec_iter iter;

	for (const char *const *arg = proc->map->argv; ; arg++) {
		argstr = (char *) get_user_uintptr(&arg);
		if (!argstr) {
			break;
		}
		int result = memory_map_load_pages_into_kvec(proc->map, kvec, 10, (virtual_address_t) argstr, max - i, 0);
		if (result < 0) {
			return result;
		}
		iovec_iter_init_kvec(&iter, kvec, result);
		result = kvec_strncpy_out_of_iter(kvec, result, 0, &buffer[i], max - i);
		if (result < 0) {
			return result;
		}
		i += result + 1;
		buffer[i - 1] = ' ';
		if (i > max) {
			break;
		}
	}
	buffer[i - 1] = '\0';
	return i;
}


int get_data_stat(struct process *proc, char *buffer, int max)
{
	char *cmd;

	#if defined(CONFIG_MMU)
	int result;
	uintptr_t arg;
	struct kvec kvec[10];
	char cmd_buffer[32];

	if (proc->map->argv) {
		result = memory_map_load_pages_into_kvec(proc->map, kvec, 10, (virtual_address_t) proc->map->argv, sizeof(uintptr_t), 0);
		if (result < 0)
			return result;
		result = kvec_memcpy_out_of_iter(kvec, result, 0, &arg, sizeof(uintptr_t));

		result = memory_map_load_pages_into_kvec(proc->map, kvec, 10, (virtual_address_t) arg, 32, 0);
		if (result < 0)
			return result;
		result = kvec_strncpy_out_of_iter(kvec, result, 0, cmd_buffer, 32);
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

