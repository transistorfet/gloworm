
#ifndef _SRC_KERNEL_FS_EXT2_BITMAPS_H
#define _SRC_KERNEL_FS_EXT2_BITMAPS_H

#include <string.h>
#include <kernel/fs/bufcache.h>

#include "ext2.h"

typedef int bitnum_t;

static inline char bit_mask(char bits)
{
	char byte = 0x00;
	for (short j = 0; j < bits; j++)
		byte = (byte << 1) | 0x01;
	return byte;
}

static inline int bitmap_init(struct bufcache *bufcache, block_t bitmap_blocknum, int num_entries, short reserve)
{
	char *block;
	struct buf *buf;
	const int block_size = bufcache->block_size;

	buf = get_block(bufcache, bitmap_blocknum);
	if (!buf)
		return -1;
	block = buf->block;

	int bytes = (num_entries >> 3);
	char bits = (num_entries & 0x07);

	if (bits)
		bytes += 1;
	memset(block, 0x00, bytes);
	if (bits)
		block[bytes - 1] = ~bit_mask(bits);
	memset(&block[bytes], 0xFF, block_size - bytes);

	// Reserve entries at the beginning of table
	short i = 0;
	for (; i < (reserve >> 3); i++)
		block[i] = 0xFF;
	block[i] = bit_mask(reserve & 0x7);

	release_block(buf, BCF_DIRTY);
	return 0;
}

static inline bitnum_t bit_alloc(struct bufcache *bufcache, block_t bitmap_blocknum, block_t near)
{
	char bit;
	char *data;
	struct buf *buf;
	block_t block = 0;
	const int block_size = bufcache->block_size;

	buf = get_block(bufcache, bitmap_blocknum);
	if (!buf)
		return 0;
	data = buf->block;

	for (int i = 0; i < block_size; i++) {
		//printk("Bitsearch %d: %x\n", i, block[i]);
		if ((char) ~data[i]) {
			for (bit = 0; bit < 8 && ((0x01 << bit) & data[i]); bit++) { }
			data[i] |= (0x01 << bit);
			release_block(buf, BCF_DIRTY);
			return bit + (i * 8) + (block * block_size * 8);
		}
	}
	release_block(buf, 0);
	return 0;
}

static inline int bit_free(struct bufcache *bufcache, block_t bitmap_blocknum, bitnum_t bitnum)
{
	int byte = (bitnum >> 3);
	char bit = (bitnum & 0x7);
	struct buf *buf = get_block(bufcache, bitmap_blocknum);
	if (!buf)
		return -1;

	((char *) buf->block)[byte] &= ~(0x01 << bit);
	release_block(buf, BCF_DIRTY);
	return 0;
}

#endif
