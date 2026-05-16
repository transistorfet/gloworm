
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

/*
static inode_t alloc_inode(struct mount *mp, mode_t mode, uid_t uid, gid_t gid, device_t rdev)
{
	bitnum_t inode_num;
	struct buf *inode_buf;
	struct ext2_super *super;
	struct ext2_inode *inode_table;
	int block_size = mp->bufcache.block_size;

	super = EXT2_SUPER(mp->super);

	for (int group = 0; group < super->num_groups; group++) {
		inode_num = bit_alloc(&mp->bufcache, super->groups[group].inode_bitmap, super->super.inodes_per_group, 0);
		if (!inode_num)
			return ENOSPC;

		super->groups[group].free_inode_count -= 1;
		super->super.total_unalloc_inodes -= 1;

		// TODO this needs to be fixed
		short inode_offset = (inode_num - 1) & (EXT2_INODES_PER_BLOCK(block_size) - 1);
		block_t block_offset = (inode_num - 1) >> EXT2_LOG_INODES_PER_BLOCK(block_size);
		inode_buf = get_block(&mp->bufcache, EXT2_INODE_TABLE_START(&super->super) + block_offset);
		if (!inode_buf)
			return ENOMEM;

		inode_table = inode_buf->block;
		inode_table[inode_offset].mode = htole16(mode);
		inode_table[inode_offset].uid = htole16(uid);
		inode_table[inode_offset].size = 0;
		inode_table[inode_offset].gid = (uint8_t) gid;
		inode_table[inode_offset].nlinks = (uint8_t) 1;
		inode_table[inode_offset].mtime = htole32(get_system_time());
		for (short j = 0; j < EXT2_INODE_BLOCKNUMS; j++)
			inode_table[inode_offset].blocks[j] = NULL;
		if (S_ISCHR(mode))
			inode_table[inode_offset].blocks[0] = htole16(rdev);

		release_block(inode_buf, BCF_DIRTY);

		return (super->super.inodes_per_group * group) + inode_num;
	}

	return ENOSPC;
}

static int free_inode(struct mount *mp, inode_t ino)
{
	int error;
	struct ext2_super *super;

	super = EXT2_SUPER(mp->super);
	error = bit_free(&mp->bufcache, EXT2_INODE_BITMAP_START(&super->super), ino);
	if (error < 0)
		return error;
	super->groups[group].free_inode_count += 1;
	super->super.total_unalloc_inodes += 1;
}
*/

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
