
#ifndef _SRC_KERNEL_FS_MINIX_SUPER_H
#define _SRC_KERNEL_FS_MINIX_SUPER_H

#include <endian.h>
#include <kernel/printk.h>
#include <kernel/drivers.h>
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


static inline int read_superblock(device_t dev, char *buffer)
{
	int size;
	struct kvec kvec;
	struct iovec_iter iter;

	// Initialize the bufcache with a safe default in order to fetch the superblock itself
	iovec_iter_init_simple_kvec(&iter, &kvec, buffer, MINIX_SUPERBLOCK_SIZE);
	size = dev_read(dev, MINIX_V1_SUPER_ZONE * MINIX_V1_ZONE_SIZE, &iter);
	if (size != 512) {
		return EIO;
	}
	return 0;
}

static int load_superblock(struct mount *mp)
{
	int error = 0;
	struct minix_super *super;
	struct minix_v1_superblock *super_v1;
	char super_buf[MINIX_SUPERBLOCK_SIZE];

	error = read_superblock(mp->dev, super_buf);
	if (error < 0)
		return error;
	super_v1 = (struct minix_v1_superblock *) super_buf;

	// TODO this is a temporary hack for cold starting a ram disk
	if (le16toh(super_v1->magic) != 0x137F) {
		log_notice("minixfs: initializing root disk\n");
		error = minix_mkfs(mp->dev);
		if (error < 0) {
			return EINVAL;
		}
	}

	super = kmalloc(sizeof(struct minix_super));
	memcpy(&super->super_v1, super_v1, sizeof(struct minix_v1_superblock));
	superblock_from_le(&super->super_v1);
	super->max_filename = MINIX_V1_MAX_FILENAME;

	mp->super = super;
	mp->block_size = 1024 << super->super_v1.log_zone_size;

	// Initialize the bufcache with the actual block size from the superblock
	init_bufcache(&mp->bufcache, mp->dev, mp->block_size);

	// TODO do you need to modify the state value stored in the superblock and write it?
	//	which would be used to detect an unclean unmount
	return 0;
}

static void free_superblock(struct minix_super *super)
{
	kmfree(super);
}

#endif

