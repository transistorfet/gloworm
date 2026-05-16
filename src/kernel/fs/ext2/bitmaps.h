
#ifndef _SRC_KERNEL_FS_EXT2_BITMAPS_H
#define _SRC_KERNEL_FS_EXT2_BITMAPS_H

#include <string.h>
#include <kernel/fs/bufcache.h>

#include "ext2.h"

/*
struct bitmap {
	device_t dev;
	ext2_block_t start;
	ext2_block_t end;
	short words_per_block;
};
*/

typedef int bitnum_t;

static inline char bit_mask(char bits)
{
	char byte = 0x00;
	for (short j = 0; j < bits; j++)
		byte = (byte << 1) | 0x01;
	return byte;
}

static inline int bitmap_init(struct bufcache *bufcache, block_t bitmap_start, int bitmap_size, int num_entries, short reserve)
{
	char *block;
	struct buf *buf;
	int block_size = bufcache->block_size;

	// Set only num_entries bits as free, and the rest of the block as reserved (unallocatable)
	for (short i = bitmap_start; i < bitmap_start + bitmap_size; i++) {
		buf = get_block(bufcache, i);
		if (!buf)
			return -1;
		block = buf->block;

		if (num_entries < EXT2_BITS_PER_BLOCK(block_size)) {
			int bytes = (num_entries >> 3);
			char bits = (num_entries & 0x07);

			if (bits)
				bytes += 1;
			memset(block, 0x00, bytes);
			if (bits)
				block[bytes - 1] = ~bit_mask(bits);
			memset(&block[bytes], 0xFF, block_size - bytes);
			break;
		} else {
			memset(block, 0x00, block_size);
		}
		num_entries -= EXT2_BITS_PER_BLOCK(block_size);

		release_block(buf, BCF_DIRTY);
	}

	buf = get_block(bufcache, bitmap_start);
	if (!buf)
		return -1;
	block = buf->block;

	// Reserve entries at the beginning of table
	short i = 0;
	for (; i < (reserve >> 3); i++)
		block[i] = 0xFF;
	block[i] = bit_mask(reserve & 0x7);

	release_block(buf, BCF_DIRTY);
	return 0;
}

static inline bitnum_t bit_alloc(struct bufcache *bufcache, block_t bitmap_start, int bitmap_size, block_t near)
{
	char bit;
	char *data;
	struct buf *buf;
	block_t block = 0;
	int block_size = bufcache->block_size;

	// TODO this can be simplified because a group always only has one block for a bitmap rather than multiple
	for (; block < bitmap_size; block++) {
		buf = get_block(bufcache, bitmap_start + block);
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
	}

	return 0;
}

static inline int bit_free(struct bufcache *bufcache, block_t bitmap_start, bitnum_t bitnum)
{
	// TODO this can be simplified because a group always only has one block for a bitmap rather than multiple
	block_t block = (bitnum >> 3) / bufcache->block_size;
	int i = (bitnum >> 3);
	char bit = (bitnum & 0x7);
	struct buf *buf = get_block(bufcache, bitmap_start + block);
	if (!buf)
		return -1;
	char *data = buf->block;

	data[i] &= ~(0x01 << bit);
	release_block(buf, BCF_DIRTY);
	return 0;
}

#endif
