
#include <stddef.h>

#include <errno.h>
#include <kernel/fs/vfs.h>
#include <kernel/mm/kmalloc.h>
#include <kernel/proc/filedesc.h>


struct fd_table *alloc_fd_table()
{
	struct fd_table *table;

	table = kzalloc(sizeof(struct fd_table));
	init_fd_table(table);
	return table;
}

void free_fd_table(struct fd_table *table)
{
	kmfree(table);
}

void init_fd_table(struct fd_table *table)
{
	for (short i = 0; i < OPEN_MAX; i++) {
		table->files[i] = NULL;
	}
}

void release_fd_table(struct fd_table *table)
{
	for (short i = 0; i < OPEN_MAX; i++) {
		if (table->files[i]) {
			vfs_close(table->files[i]);
		}
	}
}

void dup_fd_table(struct fd_table *dest, struct fd_table *source)
{
	for (short i = 0; i < OPEN_MAX; i++) {
		if (source->files[i]) {
			// TODO this needs to be replaced with a new vfs_duplicate() function
			dest->files[i] = vfs_clone_fileptr(source->files[i]);
		}
	}
}

int find_unused_fd(struct fd_table *table)
{
	for (short i = 0; i < OPEN_MAX; i++) {
		if (!table->files[i]) {
			return i;
		}
	}
	return EMFILE;
}

struct vfile *get_fd(struct fd_table *table, int fd)
{
	if (fd >= OPEN_MAX || !table->files[fd]->vnode) {
		return NULL;
	}
	return table->files[fd];
}

void set_fd(struct fd_table *table, int fd, struct vfile *file)
{
	if (fd >= OPEN_MAX) {
		return;
	}
	table->files[fd] = file;
}

void dup_fd(struct fd_table *table, int fd, struct vfile *file)
{
	if (fd >= OPEN_MAX) {
		return;
	}
	table->files[fd] = vfs_clone_fileptr(file);
}

void unset_fd(struct fd_table *table, int fd)
{
	set_fd(table, fd, NULL);
}

