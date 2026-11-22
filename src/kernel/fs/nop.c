
#include <errno.h>
#include <attributes.h>
#include <kernel/fs/nop.h>
#include <kernel/fs/vfs.h>


int nop_mkfs(__unused device_t dev, __unused const struct mkfs_options *opts)
{
	return ENOENT;
}

int nop_sync(__unused struct mount *mp)
{
	return 0;
}

int nop_create(__unused struct vnode *vnode, __unused const char *filename, __unused mode_t mode, __unused uid_t uid, __unused struct vnode **result)
{
	return EPERM;
}

int nop_mknod(__unused struct vnode *vnode, __unused const char *name, __unused mode_t mode, __unused device_t dev, __unused uid_t uid, __unused struct vnode **result)
{
	return EPERM;
}

int nop_lookup(__unused struct vnode *vnode, __unused const char *name, __unused struct vnode **result)
{
	return ENOENT;
}

int nop_link(__unused struct vnode *oldvnode, __unused struct vnode *newparent, __unused const char *filename)
{
	return EPERM;
}

int nop_unlink(__unused struct vnode *parent, __unused struct vnode *vnode, __unused const char *filename)
{
	return EPERM;
}

int nop_rename(__unused struct vnode *vnode, __unused struct vnode *oldparent, __unused const char *oldname, __unused struct vnode *newparent, __unused const char *newname)
{
	return EPERM;
}

int nop_truncate(__unused struct vnode *vnode)
{
	return 0;
}

int nop_update(__unused struct vnode *vnode)
{
	return 0;
}

int nop_release(__unused struct vnode *vnode)
{
	return 0;
}

int nop_open(__unused struct vfile *file, __unused int flags)
{
	return EPERM;
}

int nop_close(__unused struct vfile *file)
{
	return 0;
}

int nop_read(__unused struct vfile *file, __unused struct iovec_iter *iter)
{
	return 0;
}

int nop_write(__unused struct vfile *file, __unused struct iovec_iter *iter)
{
	return 0;
}

int nop_ioctl(__unused struct vfile *file, __unused unsigned int request, __unused struct iovec_iter *iter, __unused uid_t uid)
{
	return EPERM;
}

int nop_poll(__unused struct vfile *file, __unused int events)
{
	return events & (VFS_POLL_READ | VFS_POLL_WRITE);
}

off_t nop_seek(__unused struct vfile *file, __unused off_t position, __unused int whence)
{
	return 0;
}

int nop_readdir(__unused struct vfile *file, __unused struct dirent *dir)
{
	return 0;
}

