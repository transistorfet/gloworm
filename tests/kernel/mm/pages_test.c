
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "../src/kernel/mm/pages.c"

#define NATIVE_BASE	0x200000

int test_page_single_allocation(void)
{
	const size_t SIZE = 0x6000;

	#define ADDR(addr)	(base + (addr))

	#ifdef HOST_TARGET
	uint8_t *base = malloc(SIZE);
	#else
	uint8_t *base = (uint8_t *) NATIVE_BASE;
	#endif

	struct page_block block;

	init_page_block(&block, (void *) base, SIZE);

	// NOTE: ADDR(0x1000) should be the first non-bitmap page, and there should only be 7 available pages
	assert(page_block_alloc_single(&block) == (void *) ADDR(0x1000));
	assert(page_block_alloc_single(&block) == (void *) ADDR(0x2000));
	assert(page_block_alloc_single(&block) == (void *) ADDR(0x3000));
	assert(page_block_alloc_single(&block) == (void *) ADDR(0x4000));
	page_block_free_single(&block, (page_t *) ADDR(0x4000));
	assert(page_block_alloc_single(&block) == (void *) ADDR(0x4000));
	assert(page_block_alloc_single(&block) == (void *) ADDR(0x5000));
	// Out of memory because it's too big and there's only one page left
	assert(page_block_alloc_single(&block) == (void *) 0);

	page_block_free_single(&block, (page_t *) ADDR(0x3000));
	page_block_free_single(&block, (page_t *) ADDR(0x4000));
	assert((uint8_t *) page_block_alloc_single(&block) == ADDR(0x3000));
	assert((uint8_t *) page_block_alloc_single(&block) == ADDR(0x4000));
	assert((uint8_t *) page_block_alloc_single(&block) == 0);

	return 0;
}

int test_page_contiguous_allocation(void)
{
	const size_t SIZE = 0x8000;

	#define ADDR(addr)	(base + (addr))

	#ifdef HOST_TARGET
	uint8_t *base = malloc(SIZE);
	#else
	uint8_t *base = (uint8_t *) NATIVE_BASE;
	#endif

	struct page_block block;

	init_page_block(&block, (void *) base, SIZE);

	// NOTE: ADDR(0x1000) should be the first non-bitmap page, and there should only be 7 available pages
	assert(page_block_alloc_contiguous(&block, 1) == (void *) ADDR(0x1000));
	assert(page_block_alloc_contiguous(&block, 2) == (void *) ADDR(0x2000));
	assert(page_block_alloc_contiguous(&block, 3) == (void *) ADDR(0x4000));
	// Out of memory because it's too big and there's only one page left
	assert(page_block_alloc_contiguous(&block, 4) == (void *) 0);

	// Verify that there's still one page left
	assert(page_block_alloc_contiguous(&block, 1) == (void *) ADDR(0x7000));

	// Confirm we're now out of memory
	assert(page_block_alloc_contiguous(&block, 1) == (void *) 0);

	page_block_free_contiguous(&block, (page_t *) ADDR(0x3000), 2 * 0x1000);
	assert((uint8_t *) page_block_alloc_contiguous(&block, 2) == ADDR(0x3000));
	assert((uint8_t *) page_block_alloc_contiguous(&block, 2) == 0);

	return 0;
}

#define run(test) \
	printf("Running %s\n", #test); \
	assert(test() == 0);

int main()
{
	run(test_page_single_allocation);
	run(test_page_contiguous_allocation);

	printf("All tests completed\n");
}
