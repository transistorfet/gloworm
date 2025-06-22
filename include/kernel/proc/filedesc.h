
#ifndef _KERNEL_PROC_FILEDESC_H
#define _KERNEL_PROC_FILEDESC_H

#define OPEN_MAX	20

#include <kernel/fs/vfs.h>

struct fd_table {
	int refcount;
	struct vfile *files[OPEN_MAX];
};

struct fd_table *alloc_fd_table();
void free_fd_table(struct fd_table *table);
void init_fd_table(struct fd_table *table);
void release_fd_table(struct fd_table *table);
void dup_fd_table(struct fd_table *dest, struct fd_table *source);

int find_unused_fd(struct fd_table *table);
struct vfile *get_fd(struct fd_table *table, int fd);
void set_fd(struct fd_table *table, int fd, struct vfile *file);
void dup_fd(struct fd_table *table, int fd, struct vfile *file);
void unset_fd(struct fd_table *table, int fd);

static inline struct fd_table *make_ref_fd_table(struct fd_table *table)
{
	table->refcount++;
	return table;
}

#endif
