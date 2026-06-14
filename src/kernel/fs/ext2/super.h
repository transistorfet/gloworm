
#ifndef _SRC_KERNEL_FS_EXT2_SUPER_H
#define _SRC_KERNEL_FS_EXT2_SUPER_H

#include <endian.h>
#include <kernel/printk.h>
#include <kernel/drivers.h>
#include <kernel/mm/kmalloc.h>
#include <kernel/fs/bufcache.h>
#include <kernel/utils/macros.h>

#include "ext2.h"
#include "ext2structs.h"

#define EXT2_MAGIC				0xEF53

#define EXT2_INCOMPAT_FILE_TYPE_IN_DIRS		0x00002
#define EXT2_INCOMPAT_FS_NEEDS_RECOVERY		0x00004
#define EXT2_INCOMPAT_FLEX_BLOCK_GROUP		0x00200

#define EXT2_INCOMPAT_SUPPORTED			EXT2_INCOMPAT_FILE_TYPE_IN_DIRS

static inline void superblock_to_from_le(struct ext2_superblock *super)
{
	super->total_inodes = le32toh(super->total_inodes);
	super->total_blocks = le32toh(super->total_blocks);
	super->reserved_su_blocks = le32toh(super->reserved_su_blocks);
	super->total_unalloc_blocks = le32toh(super->total_unalloc_blocks);
	super->total_unalloc_inodes = le32toh(super->total_unalloc_inodes);
	super->superblock_block = le32toh(super->superblock_block);
	super->log_block_size = le32toh(super->log_block_size);
	super->log_fragment_size = le32toh(super->log_fragment_size);

	super->blocks_per_group = le32toh(super->blocks_per_group);
	super->fragments_per_block = le32toh(super->fragments_per_block);
	super->inodes_per_group = le32toh(super->inodes_per_group);

	super->last_mount_time = le32toh(super->last_mount_time);
	super->last_write_time = le32toh(super->last_write_time);

	super->mounts_since_check = le16toh(super->mounts_since_check);
	super->mounts_before_check = le16toh(super->mounts_before_check);
	super->magic = le16toh(super->magic);
	super->state = le16toh(super->state);

	super->errors = le16toh(super->errors);
	super->minor_version = le16toh(super->minor_version);
	super->last_check = le32toh(super->last_check);
	super->check_interval = le32toh(super->check_interval);
	super->creator_os = le32toh(super->creator_os);
	super->major_version = le32toh(super->major_version);

	super->reserved_uid = le16toh(super->reserved_uid);
	super->reserved_gid = le16toh(super->reserved_gid);

	super->extended.first_non_reserved_inode = le32toh(super->extended.first_non_reserved_inode);
	super->extended.inode_size = le16toh(super->extended.inode_size);
	super->extended.blockgroup_of_super = le16toh(super->extended.blockgroup_of_super);
	super->extended.compat_features = le32toh(super->extended.compat_features);
	super->extended.incompat_features = le32toh(super->extended.incompat_features);
	super->extended.ro_compat_features = le32toh(super->extended.ro_compat_features);
}

static inline int read_superblock(device_t dev, char *buffer)
{
	int size;
	struct kvec kvec;
	struct iovec_iter iter;

	// Initialize the bufcache with a safe default in order to fetch the superblock itself
	iovec_iter_init_simple_kvec(&iter, &kvec, buffer, EXT2_SUPERBLOCK_SIZE);
	size = dev_read(dev, EXT2_FIRST_SUPERBLOCK_BYTE_OFFSET, &iter);
	if (size != 512) {
		return EIO;
	}
	return 0;
}

static int load_superblock(struct mount *mp)
{
	int error;
	struct ext2_super *super = NULL;
	struct ext2_superblock *super_on_disk;
	char super_buf[EXT2_SUPERBLOCK_SIZE];

	error = read_superblock(mp->dev, super_buf);
	if (error < 0)
		return error;
	super_on_disk = (struct ext2_superblock *) super_buf;

	if (le16toh(super_on_disk->magic) != EXT2_MAGIC) {
		log_error("%s: error reading magic, expected %x but got %x\n", mp->ops->fstype, EXT2_MAGIC, le16toh(super_on_disk->magic));
		return EINVAL;
	}

	const int total_blocks = le32toh(super_on_disk->total_blocks);
	const int blocks_per_group = le32toh(super_on_disk->blocks_per_group);
	const int num_groups = (total_blocks / blocks_per_group) + (total_blocks & (blocks_per_group - 1) ? 1 : 0);

	if (num_groups <= 0) {
		log_error("%s: error mounting filesystem, calculated 0 block groups\n", mp->ops->fstype);
	}

	super = kmalloc(sizeof(struct ext2_super) + (num_groups * sizeof(struct ext2_block_group)));
	super->groups = (struct ext2_block_group *) (super + 1);	// the area just after the superblock, allocated along with it

	memcpy(&super->super, super_on_disk, sizeof(struct ext2_superblock));
	superblock_to_from_le(&super->super);

	if (super->super.log_block_size != super->super.log_fragment_size) {
		log_error("%s: block size and fragment size don't match", mp->ops->fstype);
		goto fail;
	}

	if ((super->super.extended.incompat_features & ~EXT2_INCOMPAT_SUPPORTED) != 0) {
		log_error("%s: this filesystem has incompatible features than aren't supported: %x", mp->ops->fstype, super->super.extended.incompat_features & ~EXT2_INCOMPAT_SUPPORTED);
		goto fail;
	}

	mp->super = super;
	mp->block_size = 1024 << super->super.log_block_size;
	init_bufcache(&mp->bufcache, mp->dev, mp->block_size);

	super->log_block_size = super->super.log_block_size + 10;
	super->log_inode_size = __builtin_ctz(super->super.major_version >= 1 ? super->super.extended.inode_size : 128);
	super->log_inodes_per_group = __builtin_ctz(super->super.inodes_per_group);
	super->log_inodes_per_block = __builtin_ctz(mp->block_size >> super->log_inode_size);
	super->log_blocks_per_group = __builtin_ctz(super->super.blocks_per_group);
	super->log_blocknums_per_block = __builtin_ctz(EXT2_BLOCKNUMS_PER_BLOCK(mp->block_size));
	super->num_groups = num_groups;

	// Load the block groups, starting with the first block after the superblock
	struct buf *buf = NULL;
	struct ext2_group_descriptor *group_on_disk;
	size_t offset = mp->block_size; // force an update on the first iteration
	ext2_block_t descriptor_block_num = super->super.superblock_block; // start at the superblock, since it will be incremented on the first iteration
	for (int group = 0; group < num_groups; group++, offset += sizeof(struct ext2_group_descriptor)) {
		if (offset >= mp->block_size) {
			if (buf) {
				release_block(buf, 0);
			}

			offset = 0;
			descriptor_block_num += 1;
			buf = get_block(&mp->bufcache, descriptor_block_num);
			if (!buf) {
				goto fail;
			}
		}

		group_on_disk = (struct ext2_group_descriptor *) &((char *) buf->block)[offset];
		super->groups[group].block_bitmap = le32toh(group_on_disk->block_bitmap);
		super->groups[group].inode_bitmap = le32toh(group_on_disk->inode_bitmap);
		super->groups[group].inode_table = le32toh(group_on_disk->inode_table);
		super->groups[group].free_block_count = le16toh(group_on_disk->free_block_count);
		super->groups[group].free_inode_count = le16toh(group_on_disk->free_inode_count);
		super->groups[group].used_dirs_count = le16toh(group_on_disk->used_dirs_count);

	}
	if (buf) {
		release_block(buf, 0);
	}

	// TODO run some integrity checks on the groups
	//		- check that the superblock free counts match the sum of all the group free counts
	//		- count the bits in the bitmaps and check that they match the descriptor counts

	// TODO this should actually modify the state value stored in the superblock and write it?
	return 0;

fail:
	if (super)
		kmfree(super);
	return EINVAL;
}

static void free_superblock(struct ext2_super *super)
{
	kmfree(super);
}

static int sync_superblock(struct bufcache *bufcache, struct ext2_super *super)
{
	struct buf *buf = NULL;

	buf = get_block(bufcache, super->super.superblock_block);
	if (!buf) {
		// TODO what do if error?
		return EIO;
	}
	const int super_offset = (super->super.superblock_block == 0) ? EXT2_FIRST_SUPERBLOCK_BYTE_OFFSET : 0;
	struct ext2_superblock *super_on_disk = (struct ext2_superblock *) &((char *) buf->block)[super_offset];

	memcpy(super_on_disk, &super->super, sizeof(struct ext2_superblock));
	superblock_to_from_le(super_on_disk);

	struct ext2_group_descriptor *group_on_disk;
	size_t offset = bufcache->block_size; // force an update on the first iteration
	ext2_block_t descriptor_block_num = super->super.superblock_block; // start at the superblock, since it will be incremented on the first iteration
	for (int group = 0; group < super->num_groups; group++, offset += sizeof(struct ext2_group_descriptor)) {
		if (offset >= bufcache->block_size) {
			if (buf) {
				release_block(buf, BF_DIRTY);
			}

			offset = 0;
			descriptor_block_num += 1;
			buf = get_block(bufcache, descriptor_block_num);
			if (!buf) {
				// TODO what do if error?
				return EIO;
			}
		}

		group_on_disk = (struct ext2_group_descriptor *) &((char *) buf->block)[offset];
		group_on_disk->block_bitmap = htole32(super->groups[group].block_bitmap);
		group_on_disk->inode_bitmap = htole32(super->groups[group].inode_bitmap);
		group_on_disk->inode_table = htole32(super->groups[group].inode_table);
		group_on_disk->free_block_count = htole16(super->groups[group].free_block_count);
		group_on_disk->free_inode_count = htole16(super->groups[group].free_inode_count);
		group_on_disk->used_dirs_count = htole16(super->groups[group].used_dirs_count);
	}
	if (buf) {
		release_block(buf, BF_DIRTY);
	}
	return 0;
}

#endif

