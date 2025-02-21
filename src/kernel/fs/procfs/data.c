
#include <stdio.h>
#include <string.h>

#include "data.h"

#include "../../proc/process.h"

static inline char get_proc_state(struct process *proc);
static inline size_t get_proc_size(struct process *proc);


int get_data_cmdline(struct process *proc, char *buffer, int max)
{
	int i = 0;
	for (const char **arg = proc->cmdline; *arg; arg++) {
		strncpy(&buffer[i], *arg, max - i);
		i += strlen(*arg) + 1;
		buffer[i - 1] = ' ';
		if (i > max)
			break;
	}
	buffer[i - 1] = '\0';
	return i;
}


int get_data_stat(struct process *proc, char *buffer, int max)
{
	return snprintf(buffer, max,
		"%d %s %c %d %d %d %d %ld %ld\n",
		proc->pid,
		proc->cmdline[0],
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
		"%ld %lx %lx %lx %lx %lx %lx %lx\n",
		get_proc_size(proc),
		(uintptr_t) proc->map.segments[M_TEXT].base,
		proc->map.segments[M_TEXT].length,
		(uintptr_t) proc->map.segments[M_DATA].base,
		proc->map.segments[M_DATA].length,
		(uintptr_t) proc->map.segments[M_STACK].base,
		proc->map.segments[M_STACK].length,
		(uintptr_t) proc->sp
	);
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
	for (short i = 0; i < NUM_SEGMENTS; i++)
		size += proc->map.segments[i].length;
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

