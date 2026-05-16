
#ifndef _SRC_KERNEL_FS_EXT2_BLOCKS_H
#define _SRC_KERNEL_FS_EXT2_BLOCKS_H

#include <string.h>
#include <endian.h>
#include <sys/types.h>

#include <kernel/fs/vfs.h>
#include <kernel/fs/bufcache.h>
#include <kernel/utils/macros.h>

#include "ext2.h"
#include "bitmaps.h"
#include "vnodes.h"


#define MFS_LOOKUP_BLOCK		0
#define MFS_CREATE_BLOCK		1


/*
static block_t ext2_alloc_block(struct mount *mp)
{
	bitnum_t bit;
	struct buf *buf;
	struct ext2_super *super;
	int block_size = mp->block_size;

	super = EXT2_SUPER(mp->super);

	for (int group = 0; group < super->num_groups; group++) {
		// TODO this is probably not what you want, you probably want to distribute the data across block groups more evenly
		if (super->groups[group].free_block_count > 0) {
			bit = bit_alloc(&mp->bufcache, super->groups[group].block_bitmap, super->super.blocks_per_group, 0);
			if (!bit)
				return NULL;
			// TODO this is wrong for calculating the absolute block number
			bit += super->groups[group].block_bitmap;
			super->groups[group].free_block_count -= 1;
			super->super.total_unalloc_blocks -= 1;

			buf = get_block(&mp->bufcache, bit);
			if (!buf)
				return NULL;

			memset(buf->block, 0, block_size);
			release_block(buf, BCF_DIRTY);

			return bit;
		}
	}

	return ENOSPC;
}

static void ext2_free_block(struct mount *mp, block_t blocknum)
{
	int group; 
	struct ext2_super *super;

	super = EXT2_SUPER(mp->super);
	group = blocknum / super->super.blocks_per_group;
	blocknum = rounddown(blocknum, super->super.blocks_per_group);
	bit_free(&mp->bufcache, super->groups[group].block_bitmap, blocknum);
	super->groups[group].free_block_count += 1;
	super->super.total_unalloc_blocks += 1;
}
*/


static inline char block_calculate_tier(block_t *tiers, block_t znum, int block_size)
{
	if (znum < EXT2_DIRECT_BLOCKNUMS_IN_INODE) {
		tiers[0] = znum;
		return 1;
	}

	znum -= EXT2_DIRECT_BLOCKNUMS_IN_INODE;
	tiers[0] = EXT2_DIRECT_BLOCKNUMS_IN_INODE;
	if (znum < EXT2_BLOCKNUMS_PER_BLOCK(block_size)) {
		tiers[1] = znum;
		return 2;
	}

	znum -= EXT2_BLOCKNUMS_PER_BLOCK(block_size);
	tiers[0]++;
	if (znum < EXT2_BLOCKNUMS_PER_BLOCK(block_size) * EXT2_BLOCKNUMS_PER_BLOCK(block_size)) {
		tiers[1] = (znum >> EXT2_LOG_BLOCKNUMS_PER_BLOCK(block_size));
		tiers[2] = znum & (EXT2_BLOCKNUMS_PER_BLOCK(block_size) - 1);
		return 3;
	}

	// TODO a 4th tier?
	return -1;
}

static block_t block_lookup(struct vnode *vnode, block_t znum, char create)
{
	block_t ret;
	char ntiers;
	ext2_block_t *block;
	struct buf *buf = NULL;
	block_t tiers[EXT2_TIERS];
	int block_size = vnode->mp->block_size;

	ntiers = block_calculate_tier(tiers, znum, block_size);
	if (ntiers < 0)
		return 0;

	for (short tier = 0; tier < ntiers; tier++) {
		if (tier == 0) {
			block = &EXT2_DATA(vnode).blocks[tiers[0]];
		} else {
			if (buf)
				release_block(buf, 0);
			buf = get_block(&vnode->mp->bufcache, le32toh(*block));
			if (!buf)
				return 0;

			block = &EXT2_BLOCKNUM_TABLE(buf->block)[tiers[tier]];
		}

		if (!*block) {
			if (create) {
				// TODO uncomment when working
				//*block = htole32(ext2_alloc_block(vnode->mp));
				//if (buf) {
				//	mark_block_dirty(buf);
				//}
			} else {
				if (buf)
					release_block(buf, 0);
				return 0;
			}
		}
	}

	ret = (block_t) le32toh(*block);
	if (buf)
		release_block(buf, 0);
	return ret;
}

/*
static void block_free_all(struct vnode *vnode)
{
	block_t block;
	int block_size = vnode->mp->block_size;

	// NOTE this can only be used with files or empty directories

	// If this is a device file, then the block is the device number, so just return
	if (S_ISDEV(vnode->mode))
		return;

	// Traverse all blocks and free each
	for (block_t znum = 0; (block = block_lookup(vnode, znum, MFS_LOOKUP_BLOCK)) != 0; znum++)
		ext2_free_block(vnode->mp, block);

	// Go through the tier 2 blocknum tables and free each
	if (EXT2_DATA(vnode).blocks[8]) {
		struct buf *buf = get_block(&vnode->mp->bufcache, le32toh(EXT2_DATA(vnode).blocks[8]));
		if (buf) {
			ext2_block_t *entries = buf->block;
			for (block_t i = 0; i < EXT2_BLOCKNUMS_PER_BLOCK(block_size); i++) {
				if (entries[i])
					ext2_free_block(vnode->mp, le32toh(entries[i]));
			}
			release_block(buf, 0);
		}
	}

	// Go through the tier 1 blocknum tables and free each
	for (short i = EXT2_TIER1_BLOCKNUMS; i < EXT2_TOTAL_BLOCKNUMS; i++) {
		if (EXT2_DATA(vnode).blocks[i])
			ext2_free_block(vnode->mp, le32toh(EXT2_DATA(vnode).blocks[i]));
	}

	// Go through the tier 0 blocknum tables (the inode blocks) and free each
	for (short i = 0; i < EXT2_TOTAL_BLOCKNUMS; i++)
		EXT2_DATA(vnode).blocks[i] = 0;

	mark_vnode_dirty(vnode);
}
*/

#endif
