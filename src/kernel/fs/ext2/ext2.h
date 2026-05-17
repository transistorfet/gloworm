
#ifndef _SRC_KERNEL_FS_EXT2_EXT2_H
#define _SRC_KERNEL_FS_EXT2_EXT2_H

#include <sys/stat.h>
#include <sys/types.h>
#include <kernel/fs/vfs.h>
#include <kernel/utils/queue.h>

#include "ext2structs.h"

#define EXT2_SUPER(super)	((struct ext2_super *) (super))
#define EXT2_BLOCK(block)	((struct ext2_block *) (block))
#define EXT2_VNODE(vnode)	((struct ext2_vnode *) (vnode))
#define EXT2_DATA(vnode)	(((struct ext2_vnode *) (vnode))->data)

#define EXT2_BF_DIRTY		0x0001

typedef uint32_t block_t;

struct ext2_block_group {
	block_t block_bitmap;
	block_t inode_bitmap;
	block_t inode_table;
	uint16_t free_block_count;
	uint16_t free_inode_count;
	uint16_t used_dirs_count;
};

struct ext2_super {
	size_t log_block_size;
	size_t log_inode_size;
	size_t log_inodes_per_block;
	size_t log_inodes_per_group;
	size_t log_blocks_per_group;
	struct ext2_superblock super;
	int num_groups;
	struct ext2_block_group *groups;
};

struct ext2_vnode_data {
	struct queue_node node;
	ext2_block_t blocks[EXT2_BLOCKNUMS_IN_INODE];
};

struct ext2_vnode {
	struct vnode vn;
	struct ext2_vnode_data data;
};


int ext2_init(void);
int ext2_mount(struct mount *mp, struct vnode *parent);
int ext2_unmount(struct mount *mp);
int ext2_sync(struct mount *mp);
int ext2_load_superblock(device_t dev);

int ext2_create(struct vnode *vnode, const char *filename, mode_t mode, uid_t uid, struct vnode **result);
int ext2_mknod(struct vnode *vnode, const char *name, mode_t mode, device_t dev, uid_t uid, struct vnode **result);
int ext2_lookup(struct vnode *vnode, const char *name, struct vnode **result);
int ext2_link(struct vnode *oldvnode, struct vnode *newparent, const char *filename);
int ext2_unlink(struct vnode *parent, struct vnode *vnode, const char *filename);
int ext2_rename(struct vnode *vnode, struct vnode *oldparent, const char *oldname, struct vnode *newparent, const char *newname);
int ext2_truncate(struct vnode *vnode);
int ext2_update(struct vnode *vnode);
int ext2_release(struct vnode *vnode);

int ext2_open(struct vfile *file, int flags);
int ext2_close(struct vfile *file);
int ext2_read(struct vfile *file, struct iovec_iter *iter);
int ext2_write(struct vfile *file, struct iovec_iter *iter);
int ext2_ioctl(struct vfile *file, unsigned int request, struct iovec_iter *iter, uid_t uid);
offset_t ext2_seek(struct vfile *file, offset_t position, int whence);
int ext2_readdir(struct vfile *file, struct dirent *dir);


#endif
