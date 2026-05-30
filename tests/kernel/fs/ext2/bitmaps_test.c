
#include <stdio.h>
#include <assert.h>

#include <kernel/fs/bufcache.h>

#include "../../../../src/kernel/fs/ext2/bitmaps.h"


#define TEST_BLOCK_SIZE		1024

extern struct buf mock_bufs[];

int test_bitmap_alloc_free(void)
{
	bitnum_t bit;
	uint32_t bits_value;
	struct bufcache bufcache;
	const block_t bitmap_blocknum = 0;

	init_bufcache(&bufcache, 0, TEST_BLOCK_SIZE);
	uint8_t *bitmap_data = mock_bufs[bitmap_blocknum].block;

	// Initialize bitmap and verify it's correct
	bitmap_init(&bufcache, bitmap_blocknum, 20, 1);
	printk_dump_bytes(bitmap_data, 4);
	assert(!memcmp(bitmap_data, "\x01\x00\xF0\xFF", 4));

	bit = bit_alloc(&bufcache, bitmap_blocknum, 0);
	assert(bit != 0);
	printk("allocated %d\n", bit);

	bit = bit_alloc(&bufcache, bitmap_blocknum, 0);
	assert(bit != 0);
	printk("allocated %d\n", bit);

	bit = bit_alloc(&bufcache, bitmap_blocknum, 0);
	assert(bit != 0);
	printk("allocated %d\n", bit);

	printk_dump_bytes(bitmap_data, 4);
	assert(!memcmp(bitmap_data, "\x0F\x00\xF0\xFF", 4));

	printk("freeing 1\n");
	bit_free(&bufcache, bitmap_blocknum, 1);
	printk_dump_bytes(bitmap_data, 4);
	assert(!memcmp(bitmap_data, "\x0D\x00\xF0\xFF", 4));

	bit = bit_alloc(&bufcache, bitmap_blocknum, 0);
	assert(bit != 0);
	printk("allocated %d\n", bit);
	printk_dump_bytes(bitmap_data, 4);
	assert(!memcmp(bitmap_data, "\x0F\x00\xF0\xFF", 4));

	return 0;
}

#define run(test) \
	printf("Running %s\n", #test); \
	assert(test() == 0);

int main()
{
	run(test_bitmap_alloc_free);

	printf("bitmap tests passed\n");
}
