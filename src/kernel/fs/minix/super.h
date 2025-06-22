
#ifndef _SRC_KERNEL_FS_MINIX_SUPER_H
#define _SRC_KERNEL_FS_MINIX_SUPER_H

#include <endian.h>
#include <kernel/printk.h>
#include <kernel/mm/kmalloc.h>
#include <kernel/fs/bufcache.h>

#include "minix.h"
#include "inodes.h"
#include "mkfs.h"

static inline void superblock_from_le(struct minix_v1_superblock *super_v1)
{
	super_v1->num_inodes = le16toh(super_v1->num_inodes);
	super_v1->num_zones = le16toh(super_v1->num_zones);
	super_v1->imap_blocks = le16toh(super_v1->imap_blocks);
	super_v1->zmap_blocks = le16toh(super_v1->zmap_blocks);
	super_v1->first_zone = le16toh(super_v1->first_zone);
	super_v1->log_zone_size = le16toh(super_v1->log_zone_size);
	super_v1->max_file_size = le32toh(super_v1->max_file_size);
	super_v1->magic = le16toh(super_v1->magic);
	super_v1->state = le16toh(super_v1->state);
}


static struct minix_super *load_superblock(device_t dev)
{
	struct buf *super_buf;
	struct minix_super *super;
	struct minix_v1_superblock *super_v1;

	super_buf = get_block(dev, MINIX_V1_SUPER_ZONE);
	if (!super_buf)
		return NULL;
	super_v1 = (struct minix_v1_superblock *) super_buf->block;

	// TODO this is a temporary hack for cold starting a ram disk
	if (le16toh(super_v1->magic) != 0x137F) {
		log_notice("minixfs: initializing root disk\n");
		if (minix_mkfs(dev))
			return NULL;
	}

	super = kmalloc(sizeof(struct minix_super));
	memcpy(&super->super_v1, super_v1, sizeof(struct minix_v1_superblock));
	superblock_from_le(&super->super_v1);
	super->dev = dev;
	super->max_filename = MINIX_V1_MAX_FILENAME;

	// TODO this should actually modify the state value stored in the superblock and write it?
	release_block(super_buf, 0);
	return super;
}

static void free_superblock(struct minix_super *super)
{
	kmfree(super);
}

#endif

