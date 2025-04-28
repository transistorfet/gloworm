
#ifndef _KERNEL_FS_NOPFS_H
#define _KERNEL_FS_NOPFS_H

#include <kernel/fs/vfs.h>

int nop_sync(struct mount *mp);
int nop_create(struct vnode *vnode, const char *filename, mode_t mode, uid_t uid, struct vnode **result);
int nop_mknod(struct vnode *vnode, const char *name, mode_t mode, device_t dev, uid_t uid, struct vnode **result);
int nop_lookup(struct vnode *vnode, const char *name, struct vnode **result);
int nop_link(struct vnode *oldvnode, struct vnode *newparent, const char *filename);
int nop_unlink(struct vnode *parent, struct vnode *vnode, const char *filename);
int nop_rename(struct vnode *vnode, struct vnode *oldparent, const char *oldname, struct vnode *newparent, const char *newname);
int nop_truncate(struct vnode *vnode);
int nop_update(struct vnode *vnode);
int nop_release(struct vnode *vnode);

int nop_open(struct vfile *file, int flags);
int nop_close(struct vfile *file);
int nop_read(struct vfile *file, char *buf, size_t nbytes);
int nop_write(struct vfile *file, const char *buf, size_t nbytes);
int nop_ioctl(struct vfile *file, unsigned int request, void *argp, uid_t uid);
int nop_poll(struct vfile *file, int events);
offset_t nop_seek(struct vfile *file, offset_t position, int whence);
int nop_readdir(struct vfile *file, struct dirent *dir);

#endif
