
#ifndef _SRC_KERNEL_FS_EXT2_MKFS_H
#define _SRC_KERNEL_FS_EXT2_MKFS_H

#include <errno.h>

#include "dir.h"

extern struct mount_ops ext2_mount_ops;

static int ext2_mkfs(device_t dev, const struct mkfs_options *opts)
{
	int error;
	struct ext2_super super;
	struct ext2_block_group groups[64];

	const int inode_size = 256;
	const int reserved_inodes = 11;
	const int block_size = opts->block_size ? opts->block_size : 4096;

	const int log_blocks_per_inode = 13 - __builtin_ctz(block_size);	// 1 inode per 8192 bytes
	const uint32_t total_blocks = opts->blocks;
	const uint32_t total_inodes = roundup_power_of_2(log_blocks_per_inode > 0 ? total_blocks >> log_blocks_per_inode : total_blocks << -log_blocks_per_inode);
	// A block group's bitmap must be one block, so the most blocks per group is block_size * 8
	const uint32_t blocks_per_group = total_blocks >= (block_size << 3) ? block_size << 3 : roundup_power_of_2(total_blocks);
	const uint32_t inodes_per_group = log_blocks_per_inode > 0 ? blocks_per_group >> log_blocks_per_inode : blocks_per_group << -log_blocks_per_inode;
	const int superblock_blocknum = (block_size == 1024) ? 1 : 0;
	const int num_groups = total_blocks / blocks_per_group + ((total_blocks % blocks_per_group) ? 1 : 0);
	const int inode_table_blocks = (inodes_per_group * inode_size) / block_size;
	const int group_descriptor_blocks = roundup(sizeof(struct ext2_group_descriptor) * num_groups, block_size) >> __builtin_ctz(block_size);

	// The total size is must enough to contain twice the number of blocks needed for the block group info, bitmaps, and inode table
	const uint32_t minimum_blocks = (superblock_blocknum + 1 + group_descriptor_blocks + 2 + inode_table_blocks) << 1;
	if (total_blocks < minimum_blocks) {
		log_error("%s: must allocate at least %d blocks on this disk, but only %d blocks were requested\n", ext2_mount_ops.fstype, minimum_blocks, total_blocks);
		return -1;
	}

	log_notice("%s: initializing %x disk\n", ext2_mount_ops.fstype, dev);

	struct mount mp = {
		.ops = &ext2_mount_ops,
		.mount_node = NULL,
		.root_node = NULL,
		.super = &super,
		.dev = dev,
		.bits = 0,
		.block_size = block_size,
		.bufcache = { 0 },
	};
	init_bufcache(&mp.bufcache, dev, block_size);

	super.super.total_blocks = total_blocks;
	super.super.total_inodes = total_inodes;
	super.super.reserved_su_blocks = 0x1999;
	super.super.total_unalloc_blocks = super.super.total_blocks;
	super.super.total_unalloc_inodes = super.super.total_inodes - reserved_inodes;
	super.super.superblock_block = superblock_blocknum;
	super.super.log_block_size = __builtin_ctz(block_size) - 10;
	super.super.log_fragment_size = super.super.log_block_size;

	super.super.blocks_per_group = blocks_per_group;
	super.super.fragments_per_block = blocks_per_group;
	super.super.inodes_per_group = inodes_per_group;

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

	super.super.extended.first_non_reserved_inode = reserved_inodes;
	super.super.extended.inode_size = inode_size;
	super.super.extended.blockgroup_of_super = 0;
	super.super.extended.compat_features = 0x38;	// dir index, resize inode, extended inode attributes
	super.super.extended.incompat_features = 0x02;	// filetype in directory
	super.super.extended.ro_compat_features = 0x03;	// large file, sparse superblock

	super.log_inode_size = __builtin_ctz(super.super.extended.inode_size);
	super.log_inodes_per_block = __builtin_ctz(block_size >> super.log_inode_size);
	super.log_inodes_per_group = __builtin_ctz(super.super.inodes_per_group);
	super.log_blocks_per_group = __builtin_ctz(super.super.blocks_per_group);
	super.num_groups = num_groups;

	for (int group = 0; group < num_groups; group++) {
		const int group_start = group * super.super.blocks_per_group;
		const int group_reserved_blocks = (groups[group].inode_table + inode_table_blocks) - group_start;
		const int group_reserved_inodes = group == 0 ? super.super.extended.first_non_reserved_inode : 0;

		groups[group].block_bitmap = group_start + superblock_blocknum + 1 + group_descriptor_blocks;
		groups[group].inode_bitmap = groups[group].block_bitmap + 1;
		groups[group].inode_table = groups[group].block_bitmap + 2;
		groups[group].free_block_count = super.super.blocks_per_group - group_reserved_blocks;
		groups[group].free_inode_count = super.super.inodes_per_group - group_reserved_inodes;
		groups[group].used_dirs_count = 0;

		if (groups[group].inode_table + inode_table_blocks > super.super.total_blocks) {
			log_error("last block group is too small\n");
			error = ENOSPC;
			goto fail;
		}

		if (group_start + super.super.blocks_per_group < super.super.total_blocks) {
			groups[group].free_block_count = super.super.total_blocks - group_start - group_reserved_blocks;
		}

		// Initialize bitmap blocks
		error = bitmap_init(&mp.bufcache, groups[group].block_bitmap, groups[group].free_block_count, group_reserved_blocks);
		if (error < 0) {
			goto fail;
		}
		error = bitmap_init(&mp.bufcache, groups[group].inode_bitmap, groups[group].free_inode_count, group_reserved_inodes);
		if (error < 0) {
			goto fail;
		}

		/*
		// Zero the inode table
		for (int i = 0; i < inode_table_blocks; i++) {
			struct buf *inode_buf = get_block(&mp.bufcache, groups[group].inode_table + i);
			if (!inode_buf) {
				error = EIO;
				goto fail;
			}
			memset(inode_buf->block, 0x00, block_size);
			release_block(inode_buf, BF_DIRTY);
		}
		*/
	}

	super.num_groups = num_groups;
	super.groups = groups;

	// Initialize the root inode

	struct ext2_vnode bad_node;
	memset(&bad_node, 0, sizeof(struct ext2_vnode));
	bad_node.vn.ops = &ext2_vnode_ops;
	bad_node.vn.mp = &mp;
	bad_node.vn.refcount = 1;
	bad_node.vn.mode = 0;
	bad_node.vn.ino = EXT2_BAD_INO;
	bad_node.vn.nlinks = 0;

	dir_setup(&bad_node.vn, NULL);
	write_inode(&bad_node.vn, bad_node.vn.ino);

	struct ext2_vnode root_node;
	memset(&root_node, 0, sizeof(struct ext2_vnode));
	root_node.vn.ops = &ext2_vnode_ops;
	root_node.vn.mp = &mp;
	root_node.vn.refcount = 1;
	root_node.vn.mode = S_IFDIR | S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
	root_node.vn.nlinks = 1;
	root_node.vn.ino = EXT2_ROOT_INO;

	dir_setup(&root_node.vn, NULL);
	write_inode(&root_node.vn, root_node.vn.ino);

	// Clear the first few blocks in cache before syncing the superblock
	for (int i = 0; i < 1 + group_descriptor_blocks; i++) {
		struct buf *buf = get_block(&mp.bufcache, i);
		if (buf) {
			memset(buf->block, 0, block_size);
			release_block(buf, BF_DIRTY);
		}
	}

	// Write the superblock and group descriptors to disk
	error = sync_superblock(&mp.bufcache, &super);
	if (error < 0) {
		goto fail;
	}

	sync_bufcache(&mp.bufcache);
	free_bufcache(&mp.bufcache);

	return 0;

fail:
	log_error("%s: failed creating filesystem\n", ext2_mount_ops.fstype);
	free_bufcache(&mp.bufcache);
	return error;
}

#endif
