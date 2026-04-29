
#ifndef _SRC_KERNEL_FS_PROCFS_PROCFS_H
#define _SRC_KERNEL_FS_PROCFS_PROCFS_H

#include <kernel/fs/vfs.h>
#include <kernel/utils/iovec.h>

struct process;

#define PROCFS_DATA(vnode)	(((struct procfs_vnode *) (vnode))->data)

#define PFN_ROOTDIR	0
#define PFN_PROCDIR	1
#define PFN_CMDLINE	2
#define PFN_STAT	3
#define PFN_STATM	4
#define PFN_MAPS	5
#define PFN_PAGEDUMP	6

#define PFN_MOUNTS	100


typedef short procfs_filenum_t;
typedef int (*procfs_data_t)(struct process *proc, char *buffer, int max);

struct procfs_dir_entry {
	procfs_filenum_t filenum;
	char *filename;
	procfs_data_t func;
};

struct procfs_data {
	pid_t pid;
	short filenum;
};

struct procfs_vnode {
	struct vnode vn;
	struct procfs_data data;
};


int procfs_init(void);
int procfs_mount(struct mount *mp, device_t dev, struct vnode *parent);
int procfs_unmount(struct mount *mp);

int procfs_lookup(struct vnode *vnode, const char *name, struct vnode **result);
int procfs_release(struct vnode *vnode);

int procfs_open(struct vfile *file, int flags);
int procfs_close(struct vfile *file);
int procfs_read(struct vfile *file, struct iovec_iter *iter);
int procfs_write(struct vfile *file, struct iovec_iter *iter);
int procfs_ioctl(struct vfile *file, unsigned int request, void *argp, uid_t uid);
offset_t procfs_seek(struct vfile *file, offset_t position, int whence);
int procfs_readdir(struct vfile *file, struct dirent *dir);

#endif
