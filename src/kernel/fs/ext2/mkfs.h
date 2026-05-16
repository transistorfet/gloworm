
#ifndef _SRC_KERNEL_FS_EXT2_MKFS_H
#define _SRC_KERNEL_FS_EXT2_MKFS_H

#include <errno.h>

#include "dir.h"

static int ext2_mkfs(device_t dev)
{
	struct buf *super_buf;
	struct bufcache bufcache;
	struct ext2_superblock *super;
	struct ext2_superblock super_cached;

	init_bufcache(&bufcache, dev, 1024 << EXT2_LOG_INODES_PER_BLOCK);
	super = &super_cached;

	super->num_inodes = 0x40;
	super->num_blocks = 0x64;
	super->imap_blocks = 1;
	super->zmap_blocks = 1;
	super->first_block = 6;
	super->log_block_size = EXT2_LOG_BLOCK_SIZE;
	super->max_file_size = 67108864;
	super->magic = 0x137F;
	super->state = 0x0001;

	// Write the superblock
	super_buf = get_block(&bufcache, EXT2_SUPER_BLOCK);
	if (!super_buf)
		return -1;
	super = (struct ext2_superblock *) super_buf->block;

	// TODO we assume this is not a disk yet, so manually initialize it
	memset(super_buf->block, 0x00, EXT2_BLOCK_SIZE);
	super->num_inodes = htole16(super_cached.num_inodes);
	super->num_blocks = htole16(super_cached.num_blocks);
	super->imap_blocks = htole16(super_cached.imap_blocks);
	super->zmap_blocks = htole16(super_cached.zmap_blocks);
	super->first_block = htole16(super_cached.first_block);
	super->log_block_size = htole16(super_cached.log_block_size);
	super->max_file_size = htole16(super_cached.max_file_size);
	super->magic = htole16(super_cached.magic);
	super->state = htole16(super_cached.state);

	release_block(super_buf, BCF_DIRTY);

	super = &super_cached;

	// Initialize bitmap blocks
	bitmap_init(dev, EXT2_INODE_BITMAP_START(super), super->imap_blocks, super->num_inodes, 2);
	bitmap_init(dev, EXT2_BLOCK_BITMAP_START(super), super->zmap_blocks, super->num_blocks, 2);

	// Zero the inode table
	for (int i = 0; i < (super->num_inodes >> EXT2_LOG_INODES_PER_BLOCK); i++) {
		struct buf *inode_buf = get_block(&bufcache, EXT2_INODE_TABLE_START(super) + i);
		if (!inode_buf)
			return ENOMEM;
		memset(inode_buf->block, 0x00, EXT2_BLOCK_SIZE);
		release_block(inode_buf, BCF_DIRTY);
	}

	inode_t root_ino = 1;
	block_t dir_block = super->first_block;

	// Initialize root inode
	struct buf *inode_buf = get_block(&bufcache, EXT2_INODE_TABLE_START(super));
	if (!inode_buf)
		return ENOMEM;
	struct ext2_inode *inode_table = inode_buf->block;

	inode_table[0].mode = htole16(S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO);
	inode_table[0].uid = htole16(SU_UID);
	inode_table[0].gid = 0;
	inode_table[0].size = htole32(0);
	inode_table[0].nlinks = htole16(1);
	inode_table[0].blocks[0] = htole16(dir_block);
	release_block(inode_buf, BCF_DIRTY);

	// Initialize root directory
	struct buf *dir_buf = get_block(&bufcache, dir_block);
	if (!dir_buf)
		return ENOMEM;
	struct ext2_dirent *entries = dir_buf->block;
	memset(dir_buf->block, 0, EXT2_BLOCK_SIZE);

	entries[0].inode = htole32((ext2_inode_t) root_ino);
	strcpy(entries[0].filename, ".");
	entries[1].inode = htole32((ext2_inode_t) root_ino);
	strcpy(entries[1].filename, "..");

	release_block(dir_buf, BCF_DIRTY);

	//sync_bufcache(&bufcache);
	free_bufcache(&bufcache);

	return 0;
}

#endif
