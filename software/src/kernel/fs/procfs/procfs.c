
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <kernel/vfs.h>
#include <kernel/driver.h>

#include "../../proc/process.h"
 
#include "procfs.h"
#include "../nop.h"

struct vfile_ops procfs_vfile_ops = {
	procfs_open,
	procfs_close,
	procfs_read,
	procfs_write,
	procfs_ioctl,
	procfs_seek,
	procfs_readdir,
};

struct vnode_ops procfs_vnode_ops = {
	&procfs_vfile_ops,
	nop_create,
	nop_mknod,
	procfs_lookup,
	nop_link,
	nop_unlink,
	nop_rename,
	nop_truncate,
	nop_update,
	procfs_release,
};

struct mount_ops procfs_mount_ops = {
	"procfs",
	procfs_init,
	procfs_mount,
	procfs_unmount,
	nop_sync,
};

struct procfs_dir_entry proc_files[] = {
	{ PFN_PROCDIR,	"." },
	{ PFN_ROOTDIR,	".." },
	{ PFN_CMDLINE,	"cmdline" },
	{ PFN_STAT,	"stat" },
	{ PFN_STATM,	"statm" },
	{ 0, NULL },
};


#define MAX_VNODES	6
#define MAX_BUFFER	256

static struct procfs_vnode vnode_table[MAX_VNODES];

static struct vnode *_find_vnode(pid_t pid, short filenum, mode_t mode, struct mount *mp);
static struct vnode *_alloc_vnode(pid_t pid, short filenum, mode_t mode, struct mount *mp);
static inline char get_proc_state(struct process *proc);
static inline size_t get_proc_size(struct process *proc);

int procfs_init()
{
	for (short i = 0; i < MAX_VNODES; i++)
		vnode_table[i].vn.refcount = 0;
	return 0;
}

int procfs_mount(struct mount *mp, device_t dev, struct vnode *parent)
{
	mp->root_node = _alloc_vnode(0, 0, S_IFDIR | 0755, mp);
	return 0;
}

int procfs_unmount(struct mount *mp)
{
	vfs_release_vnode(mp->root_node);
	return 0;
}

int procfs_lookup(struct vnode *vnode, const char *filename, struct vnode **result)
{
	pid_t pid;
	short filenum;
	struct process *proc;

	if (PROCFS_DATA(vnode).pid == 0) {
		pid = strtol(filename, NULL, 10);
		filenum = PFN_PROCDIR;

		proc = get_proc(pid);
		if (!proc)
			return ENOENT;
	}
	else if (PROCFS_DATA(vnode).filenum == PFN_PROCDIR) {
		pid = PROCFS_DATA(vnode).pid;

		filenum = -1;
		for (short i = 0; proc_files[i].filename; i++) {
			if (!strcmp(filename, proc_files[i].filename)) {
				filenum = proc_files[i].filenum;
				break;
			}
		}
		if (filenum < 0)
			return ENOENT;
	}
	else
		return ENOTDIR;

	if (*result)
		vfs_release_vnode(*result);
	*result = _find_vnode(pid, filenum, S_IFDIR | 0755, vnode->mp);
	return 0;
}

int procfs_release(struct vnode *vnode)
{
	return 0;
}

int procfs_open(struct vfile *file, int flags)
{
	if (PROCFS_DATA(file->vnode).pid == 0)
		proc_iter_start((struct process_iter *) &file->position);
	else if (PROCFS_DATA(file->vnode).filenum == 0)
		file->position = 0;
	return 0;
}

int procfs_close(struct vfile *file)
{
	return 0;
}

int procfs_read(struct vfile *file, char *buf, size_t nbytes)
{
	int limit;
	char *data;
	struct process *proc;
	char buffer[MAX_BUFFER];

	proc = get_proc(PROCFS_DATA(file->vnode).pid);
	if (!proc)
		return ENOENT;

	data = buffer;
	switch (PROCFS_DATA(file->vnode).filenum) {
	    case PFN_CMDLINE: {
		int i = 0;
		for (char **arg = proc->cmdline; *arg; arg++) {
			strncpy(&buffer[i], *arg, MAX_BUFFER - i);
			i += strlen(*arg) + 1;
			buffer[i - 1] = ' ';
			if (i > MAX_BUFFER)
				break;
		}
		buffer[i - 1] = '\0';
		break;
	    }
	    case PFN_STAT: {
		snprintf(buffer, MAX_BUFFER,
			"%d %s %c %d %d %d %d %d %d\n",
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
		break;
	    }
	    case PFN_STATM: {
		snprintf(buffer, MAX_BUFFER,
			"%d %x %x %x %x %x\n",
			get_proc_size(proc),
			proc->map.segments[M_TEXT].base,
			proc->map.segments[M_TEXT].length,
			proc->map.segments[M_STACK].base,
			proc->map.segments[M_STACK].length,
			proc->sp
		);
		break;
	    }
	    default:
		return 0;
	}

	limit = strlen(data);
	if (file->position + nbytes >= limit)
		nbytes = limit - file->position;
	if (nbytes)
		strncpy(buf, &data[file->position], nbytes);
	file->position += nbytes;
	return nbytes;
}

int procfs_write(struct vfile *file, const char *buf, size_t nbytes)
{
	return 0;
}

int procfs_ioctl(struct vfile *file, unsigned int request, void *argp)
{
	return -1;
}

offset_t procfs_seek(struct vfile *file, offset_t position, int whence)
{
	return -1;
}

int procfs_readdir(struct vfile *file, struct dirent *dir)
{
	struct process *proc;

	if (!(file->vnode->mode & S_IFDIR))
		return ENOTDIR;

	if (PROCFS_DATA(file->vnode).pid == 0) {
		proc = proc_iter_next((struct process_iter *) &file->position);
		if (!proc)
			return 0;

		dir->d_ino = (proc->pid << 8) | 0;
		snprintf(dir->d_name, VFS_FILENAME_MAX, "%d", proc->pid);
		dir->d_name[VFS_FILENAME_MAX - 1] = '\0';
	}
	else if (PROCFS_DATA(file->vnode).filenum == PFN_PROCDIR) {
		if (!proc_files[file->position].filename)
			return 0;
		dir->d_ino = (PROCFS_DATA(file->vnode).pid << 8) | proc_files[file->position].filenum;
		strncpy(dir->d_name, proc_files[file->position].filename, VFS_FILENAME_MAX);
		dir->d_name[VFS_FILENAME_MAX - 1] = '\0';
		file->position += 1;
	}
	else
		return ENOTDIR;
	return 1;
}


static struct vnode *_find_vnode(pid_t pid, short filenum, mode_t mode, struct mount *mp)
{
	for (short i = 0; i < MAX_VNODES; i++) {
		if (vnode_table[i].vn.refcount > 0 && PROCFS_DATA(&vnode_table[i]).pid == pid && PROCFS_DATA(&vnode_table[i]).filenum == filenum)
			return vfs_clone_vnode(&vnode_table[i].vn);
	}
	return _alloc_vnode(pid, filenum, mode, mp);
}

static struct vnode *_alloc_vnode(pid_t pid, short filenum, mode_t mode, struct mount *mp)
{
	for (short i = 0; i < MAX_VNODES; i++) {
		if (vnode_table[i].vn.refcount <= 0) {
			vfs_init_vnode(&vnode_table[i].vn, &procfs_vnode_ops, mp, mode, 1, 0, 0, 0, 0, 0, 0, 0);
			PROCFS_DATA(&vnode_table[i]).pid = pid;
			PROCFS_DATA(&vnode_table[i]).filenum = filenum;
			return &vnode_table[i].vn;
		}
	}
	return NULL;
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
	for (char i = 0; i < NUM_SEGMENTS; i++)
		size += proc->map.segments[i].length;
	return size;
}

