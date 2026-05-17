
#ifndef _SRC_KERNEL_FS_EXT2_INODES_H
#define _SRC_KERNEL_FS_EXT2_INODES_H

#include <errno.h>
#include <string.h>
#include <endian.h>
#include <sys/stat.h>

#include <kernel/time.h>
#include <kernel/fs/vfs.h>
#include <kernel/utils/macros.h>

#include "ext2.h"
#include "super.h"
#include "bitmaps.h"

static inline int get_inode_group(struct mount *mp, inode_t ino)
{
	const struct ext2_super *super = EXT2_SUPER(mp->super);
	const int group = (ino - 1) >> super->log_inodes_per_group;
	if (group > super->num_groups) {
		return EINVAL;
	}
	return group;
}

static inline struct inode_location get_inode_block_and_offset(struct mount *mp, inode_t ino)
{
	const struct ext2_super *super = EXT2_SUPER(mp->super);
	const int group = (ino - 1) >> super->log_inodes_per_group;
	if (group > super->num_groups) {
		const struct inode_location result = { EINVAL };
		return result;
	}

	const int group_inode = alignment_offset((ino - 1), 1 << super->log_inodes_per_group);
	const block_t block_offset = group_inode >> super->log_inodes_per_block;
	const int byte_offset = alignment_offset(group_inode, 1 << super->log_inodes_per_block) << super->log_inode_size;
	const struct inode_location result = { super->groups[group].inode_table + block_offset, byte_offset };
	return result;
}

static inode_t alloc_inode(struct mount *mp, mode_t mode, uid_t uid, gid_t gid, device_t rdev)
{
	int group;
	struct buf *inode_buf;
	bitnum_t group_inode_num;
	struct ext2_super *super;
	struct ext2_inode *inode;

	super = EXT2_SUPER(mp->super);

	for (group = 0; group < super->num_groups; group++) {
		// TODO should you try to distribute allocations across block groups?
		if (super->groups[group].free_inode_count > 0) {
			break;
		}
	}
	if (group >= super->num_groups)
		return ENOSPC;

	group_inode_num = bit_alloc(&mp->bufcache, super->groups[group].inode_bitmap, 0);
	if (!group_inode_num)
		return ENOSPC;
	super->groups[group].free_inode_count -= 1;
	super->super.total_unalloc_inodes -= 1;
	const ext2_inode_t inodenum = (super->super.inodes_per_group * group) + group_inode_num + 1;

	const block_t block_offset = group_inode_num >> super->log_inodes_per_block;
	const int byte_offset = alignment_offset(group_inode_num, 1 << super->log_inodes_per_block) << super->log_inode_size;
	inode_buf = get_block(&mp->bufcache, super->groups[group].inode_table + block_offset);
	if (!inode_buf)
		return ENOMEM;

	inode = (struct ext2_inode *) &((char *) inode_buf->block)[byte_offset];
	inode->mode = htole16(mode);
	inode->uid = htole16(uid);
	inode->size = 0;
	inode->gid = (uint8_t) gid;
	inode->nlinks = (uint8_t) 1;
	inode->mtime = htole32(get_system_time());
	for (short j = 0; j < EXT2_BLOCKNUMS_IN_INODE; j++)
		inode->blocks[j] = NULL;
	if (S_ISCHR(mode))
		inode->blocks[0] = htole16(rdev);

	release_block(inode_buf, BCF_DIRTY);

	return inodenum;
}

static int free_inode(struct mount *mp, inode_t ino)
{
	int error;

	const int group = get_inode_group(mp, ino);
	struct ext2_super * const super = EXT2_SUPER(mp->super);
	error = bit_free(&mp->bufcache, super->groups[group].inode_bitmap, ino);
	if (error < 0)
		return error;
	super->groups[group].free_inode_count += 1;
	super->super.total_unalloc_inodes += 1;
	return 0;
}

static int read_inode(struct vnode *vnode, inode_t ino)
{
	struct buf *inode_buf;
	struct ext2_inode *inode;

	const struct inode_location result = get_inode_block_and_offset(vnode->mp, ino);
	inode_buf = get_block(&vnode->mp->bufcache, result.block);
	if (!inode_buf)
		return ENOMEM;

	inode = (struct ext2_inode *) &((char *) inode_buf->block)[result.offset];
	vnode->mode = le16toh(inode->mode);
	vnode->uid = le16toh(inode->uid);
	vnode->size = le32toh(inode->size);
	vnode->atime = le32toh(inode->atime);
	vnode->mtime = le32toh(inode->mtime);
	vnode->ctime = le32toh(inode->ctime);
	vnode->gid = le16toh(inode->gid);
	vnode->nlinks = le16toh(inode->nlinks);
	vnode->rdev = le16toh(S_ISDEV(vnode->mode) ? inode->blocks[0] : 0);
	for (short j = 0; j < EXT2_BLOCKNUMS_IN_INODE; j++) {
		// NOTE: the block numbers are not converted, and instead kept in
		// fs-native little endian to make block lookups easier
		EXT2_DATA(vnode).blocks[j] = inode->blocks[j];
	}

	vnode->bits &= ~VBF_DIRTY;
	release_block(inode_buf, 0);
	return 0;
}

static int write_inode(struct vnode *vnode, inode_t ino)
{
	struct buf *inode_buf;
	struct ext2_inode *inode;

	const struct inode_location result = get_inode_block_and_offset(vnode->mp, ino);
	inode_buf = get_block(&vnode->mp->bufcache, result.block);
	if (!inode_buf)
		return ENOMEM;

	inode = (struct ext2_inode *) &((char *) inode_buf->block)[result.offset];
	inode->mode = htole16(vnode->mode);
	inode->uid = htole16(vnode->uid);
	inode->size = htole32(vnode->size);
	inode->atime = htole32(vnode->atime);
	inode->mtime = htole32(vnode->mtime);
	inode->ctime = htole32(vnode->ctime);
	inode->gid = htole16(vnode->gid);
	inode->nlinks = htole16(vnode->nlinks);
	if (S_ISCHR(vnode->mode)) {
		EXT2_DATA(vnode).blocks[0] = htole16(vnode->rdev);
	}
	for (short j = 0; j < EXT2_BLOCKNUMS_IN_INODE; j++) {
		// NOTE: the block numbers are not converted, and instead kept in
		// fs-native little endian to make block lookups easier
		inode->blocks[j] = EXT2_DATA(vnode).blocks[j];
	}

	vnode->bits &= ~VBF_DIRTY;
	release_block(inode_buf, BCF_DIRTY);
	return 0;
}

#endif
