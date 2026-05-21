
#ifndef _SRC_KERNEL_FS_EXT2_MKFS_H
#define _SRC_KERNEL_FS_EXT2_MKFS_H

#include <errno.h>

#include "dir.h"

static int ext2_mkfs(device_t dev, const struct mkfs_options *opts)
{
	int error;
	struct bufcache bufcache;
	const int block_size = opts->block_size ? opts->block_size : 4096;

	init_bufcache(&bufcache, dev, block_size);

	struct ext2_super super;
	struct ext2_block_group groups[64];

	super.super.total_inodes = opts->blocks;
	super.super.total_blocks = opts->blocks;
	super.super.reserved_su_blocks = 0x1999;
	super.super.total_unalloc_blocks = opts->blocks;
	super.super.total_unalloc_inodes = opts->blocks;
	super.super.superblock_block = (block_size == 1024) ? 1 : 0;
	super.super.log_block_size = __builtin_ctz(block_size);
	super.super.log_fragment_size = __builtin_ctz(block_size);

	super.super.blocks_per_group = 0x8000;
	super.super.fragments_per_block = 0;
	super.super.inodes_per_group = 0x2000;

	super.super.last_mount_time = get_system_time();
	super.super.last_write_time = super.super.last_mount_time;

	super.super.mounts_since_check = 0;
	super.super.mounts_before_check = 0xFFFF;
	super.super.magic = EXT2_MAGIC;
	super.super.state = 0x0001;

	super.super.errors = 0;
	super.super.minor_version = 0;
	super.super.last_check = 0;
	super.super.check_interval = 0;
	super.super.major_version = 1;

	super.super.reserved_uid = 0;
	super.super.reserved_gid = 0;

	super.super.extended.first_non_reserved_inode = 12;
	super.super.extended.inode_size = 256;
	super.super.extended.blockgroup_of_super = 0;
	super.super.extended.compat_features = 0x38;	// dir index, resize inode, extended inode attributes
	super.super.extended.incompat_features = 0x02;	// filetype in directory
	super.super.extended.ro_compat_features = 0x03;	// large file, sparse superblock

	const int num_groups = opts->blocks / super.super.blocks_per_group;
	const int blocks_per_inode_table = (super.super.inodes_per_group * super.super.extended.inode_size) / block_size;

	for (int group = 0; group < num_groups; group++) {
		const int group_start = group * super.super.blocks_per_group;

		groups[group].block_bitmap = group_start + 0x21;
		groups[group].inode_bitmap = group_start + 0x22;
		groups[group].inode_table = group_start + 0x23;
		groups[group].free_block_count = super.super.blocks_per_group;
		groups[group].free_inode_count = super.super.inodes_per_group;
		groups[group].used_dirs_count = 0;

		const int group_reserved_blocks = blocks_per_inode_table + 0x23;
		const int group_reserved_inodes = group == 0 ? super.super.extended.first_non_reserved_inode - 1 : 0;

		// Initialize bitmap blocks
		bitmap_init(&bufcache, groups[group].block_bitmap, groups[group].free_block_count, group_reserved_blocks);
		bitmap_init(&bufcache, groups[group].inode_bitmap, groups[group].free_inode_count, group_reserved_inodes);

		// Zero the inode table
		for (int i = 0; i < groups[group].free_inode_count; i++) {
			struct buf *inode_buf = get_block(&bufcache, groups[group].inode_table + i);
			if (!inode_buf)
				return ENOMEM;
			memset(inode_buf->block, 0x00, block_size);
			release_block(inode_buf, BF_DIRTY);
		}
	}

	super.num_groups = num_groups;
	super.groups = groups;

	// Initialize the root inodes

	inode_t root_ino = 2;
	// TODO this is just copied from the linux-created one, but I don't know how it's derived
	block_t dir_block = 0x223;

	// Initialize root inode
	struct buf *inode_buf = get_block(&bufcache, super.groups[0].inode_table);
	if (!inode_buf)
		return ENOMEM;
	struct ext2_inode *inode_table = inode_buf->block;

	inode_table[1].mode = htole16(S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO);
	inode_table[1].uid = htole16(SU_UID);
	inode_table[1].gid = 0;
	inode_table[1].size = htole32(0);
	inode_table[1].nlinks = htole16(1);
	inode_table[1].blocks[0] = htole16(dir_block);
	release_block(inode_buf, BF_DIRTY);

	// Initialize root directory
	struct buf *dir_buf = get_block(&bufcache, dir_block);
	if (!dir_buf)
		return ENOMEM;
	memset(dir_buf->block, 0, block_size);

	const uint16_t dot_entry_len = sizeof(struct ext2_dirent) + 4;

	struct ext2_dirent *current_entry = dir_buf->block;
	current_entry->inode = htole32((ext2_inode_t) root_ino);
	current_entry->filetype = EXT2_FT_DIR;
	current_entry->entry_len = htole16(dot_entry_len);
	current_entry->name_len = 1;
	strcpy(EXT2_DIRENT_FILENAME(current_entry), ".");

	current_entry = (struct ext2_dirent *) &((char *) dir_buf->block)[dot_entry_len];
	current_entry->inode = htole32(root_ino);
	current_entry->filetype = EXT2_FT_DIR;
	current_entry->entry_len = htole16(block_size - dot_entry_len);
	current_entry->name_len = 2;
	memcpy(EXT2_DIRENT_FILENAME(current_entry), "..", current_entry->name_len);

	release_block(dir_buf, BF_DIRTY);


	// Write the superblock and group descriptors to disk
	error = sync_superblock(&bufcache, &super);
	if (error < 0)
		return error;

	sync_bufcache(&bufcache);
	free_bufcache(&bufcache);

	return 0;
}

#endif
