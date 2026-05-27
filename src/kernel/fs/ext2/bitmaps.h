
#ifndef _SRC_KERNEL_FS_EXT2_BITMAPS_H
#define _SRC_KERNEL_FS_EXT2_BITMAPS_H

#include <string.h>
#include <kernel/fs/bufcache.h>
#include <kernel/utils/macros.h>

#include "ext2.h"

typedef int bitnum_t;

static bitnum_t bit_alloc(struct bufcache *bufcache, ext2_block_t bitmap_blocknum, ext2_block_t near);

static inline char bit_mask(char bits)
{
	char byte = 0x00;
	for (short j = 0; j < bits; j++) {
		byte = (byte << 1) | 0x01;
	}
	return byte;
}

static int bitmap_init(struct bufcache *bufcache, ext2_block_t bitmap_blocknum, int num_entries, int reserve)
{
	char *block;
	struct buf *buf;

	buf = get_block(bufcache, bitmap_blocknum);
	if (!buf)
		return -1;
	block = buf->block;

	const int last_byte = num_entries >> 3;
	const int last_bits = num_entries & 0x07;
	memset(block, 0x00, last_byte);
	if (last_byte < bufcache->block_size) {
		block[last_byte] = bit_mask(last_bits);
	}
	if (last_byte + 1 < bufcache->block_size) {
		memset(&block[last_byte + 1], 0xFF, bufcache->block_size - last_byte - 1);
	}

	int i = 0;
	while (reserve > 8) {
		block[i++] = 0xFF;
		reserve -= 8;
	}
	block[i++] = bit_mask(reserve);

	release_block(buf, BF_DIRTY);
	return 0;
}

static bitnum_t bit_alloc(struct bufcache *bufcache, ext2_block_t bitmap_blocknum, ext2_block_t near)
{
	char bit;
	char *data;
	struct buf *buf;
	const int block_size = bufcache->block_size;

	buf = get_block(bufcache, bitmap_blocknum);
	if (!buf)
		return 0;
	data = buf->block;

	for (int i = 0; i < block_size; i++) {
		if ((char) ~data[i]) {
			for (bit = 0; bit < 8 && ((0x01 << bit) & data[i]); bit++) { }
			data[i] |= (0x01 << bit);
			release_block(buf, BF_DIRTY);
			return bit + (i * 8);
		}
	}
	release_block(buf, 0);
	return 0;
}

static inline int bit_free(struct bufcache *bufcache, ext2_block_t bitmap_blocknum, bitnum_t bitnum)
{
	int byte = (bitnum >> 3);
	char bit = (bitnum & 0x7);
	struct buf *buf = get_block(bufcache, bitmap_blocknum);
	if (!buf)
		return -1;

	((char *) buf->block)[byte] &= ~(0x01 << bit);
	release_block(buf, BF_DIRTY);
	return 0;
}

#endif
