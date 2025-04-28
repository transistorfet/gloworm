
#ifndef _KERNEL_FS_FILEPTR_H
#define _KERNEL_FS_FILEPTR_H

#include <kernel/fs/vfs.h>

void init_fileptr_table();
struct vfile *new_fileptr(struct vnode *vnode, int flags);
struct vfile *dup_fileptr(struct vfile *file);
void free_fileptr(struct vfile *file);

#endif

