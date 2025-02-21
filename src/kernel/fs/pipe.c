
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <kernel/kmalloc.h>
#include <kernel/syscall.h>
#include <kernel/scheduler.h>

#include "nop.h"
#include "fileptr.h"
#include "../proc/process.h"

#define PIPE_BUFFER_MAX		512

int pipe_close(struct vfile *file);
int pipe_read(struct vfile *file, char *buf, size_t nbytes);
int pipe_write(struct vfile *file, const char *buf, size_t nbytes);
int pipe_release(struct vnode *vnode);
int pipe_poll(struct vfile *file, int events);

struct vfile_ops pipe_vfile_ops = {
	nop_open,
	pipe_close,
	pipe_read,
	pipe_write,
	nop_ioctl,
	pipe_poll,
	nop_seek,
	nop_readdir,
};

struct vnode_ops pipe_vnode_ops = {
	&pipe_vfile_ops,
	nop_create,
	nop_mknod,
	nop_lookup,
	nop_link,
	nop_unlink,
	nop_rename,
	nop_truncate,
	nop_update,
	pipe_release,
};


struct pipe_data {
	char buffer[PIPE_BUFFER_MAX];
};

struct pipe_vnode {
	struct vnode vn;
	struct pipe_data data;
};


int vfs_create_pipe(struct vfile **rfile, struct vfile **wfile)
{
	struct vnode *vnode;

	vnode = kmalloc(sizeof(struct pipe_vnode));
	if (!vnode)
		return ENOMEM;
	// TODO replace uid and gid with valid values based on current proc?
	vfs_init_vnode(vnode, &pipe_vnode_ops, NULL, 0600, 1, 0, 0, 0, 0, 0, 0, 0, 0);

	// We will be creating two references to this vnode, so increament the refcount
	vfs_clone_vnode(vnode);

	// TODO can I allocate these before I create the vnode/inode?
	// Allocate two file pointers
	*rfile = new_fileptr(vnode, 0);
	if (!*rfile) {
		kmfree(vnode);
		return ENFILE;
	}
	*wfile = new_fileptr(vnode, 0);
	if (!*wfile) {
		kmfree(vnode);
		free_fileptr(*rfile);
		return ENFILE;
	}

	// TODO file pointers don't have flags atm...
	//rfile->flags = O_RDONLY
	//wfile->flags = O_WRONLY
	// TODO this could be done in new_vnode() if it'll use mode, but we need to indicate it's a pipe... linux seems to use custom vnode_ops/vfile_ops
	//vnode->mode |= I_PIPE

	return 0;
}

int pipe_release(struct vnode *vnode)
{
	kmfree(vnode);
	return 0;
}

int pipe_close(struct vfile *file)
{
	// TODO decrement listeners (what listeners was I talking about?)
	return 0;
}

int pipe_read(struct vfile *file, char *buf, size_t nbytes)
{
	register struct vnode *vnode = file->vnode;
	struct pipe_data *vnode_data = ((struct pipe_data *) &vnode->data);
	register char *buffer = vnode_data->buffer;

	if (!buffer)
		return EIO;

	// TODO Do you also need to add a check for a broken pipe here too?

	if (file->position == vnode->size) {
		suspend_current_syscall(VFS_POLL_READ);
		return 0;
	}

	if (nbytes > vnode->size - file->position)
		nbytes = vnode->size - file->position;
	memcpy(buf, &buffer[file->position], nbytes);
	file->position += nbytes;

	if (file->position == vnode->size) {
		file->position = 0;
		vnode->size = 0;
	}

	// Wake up any processes that are blocked while writing to this vnode
	resume_blocked_procs(VFS_POLL_WRITE, vnode, 0);

	return nbytes;
}

int pipe_write(struct vfile *file, const char *buf, size_t nbytes)
{
	register struct vnode *vnode = file->vnode;
	struct pipe_data *vnode_data = ((struct pipe_data *) &vnode->data);
	register char *buffer = vnode_data->buffer;

	if (!buffer)
		return EIO;

	// TODO you need to detect a broken pipe and raise SIGPIPE

	if (nbytes > PIPE_BUFFER_MAX - file->position) {
		// TODO this will work for now, but any write operation that's larger than the buffer size wont ever complete
		// Trying to write more bytes than are available in the buffer, so block until the reader has caught up
		suspend_current_syscall(VFS_POLL_WRITE);
		return 0;
		//nbytes = PIPE_BUFFER_MAX - file->position;
	}
	memcpy(&buffer[file->position], buf, nbytes);
	file->position += nbytes;

	if (file->position > vnode->size)
		vnode->size = file->position;

	// Wake up any processes that are blocked while reading from this vnode
	resume_blocked_procs(VFS_POLL_READ, vnode, 0);

	return nbytes;
}

int pipe_poll(struct vfile *file, int events)
{
	int revents = 0;
	register struct vnode *vnode = file->vnode;

	if ((events & VFS_POLL_READ) && (vnode->size - file->position > 0))
		revents |= VFS_POLL_READ;
	if ((events & VFS_POLL_WRITE) && vnode->size >= PIPE_BUFFER_MAX)
		revents |= VFS_POLL_WRITE;
	return revents;
}

