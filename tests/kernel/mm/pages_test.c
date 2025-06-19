
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <kconfig.h>

#include "../src/kernel/mm/pages.c"

#define NATIVE_BASE	0x200000

int test_page_single_allocation(void)
{
	const size_t SIZE = PAGE_SIZE * 6;

	#define ADDR(addr)	(base + (addr))

	#ifdef HOST_TARGET
	uint8_t *base = malloc(SIZE);
	#else
	uint8_t *base = (uint8_t *) NATIVE_BASE;
	#endif

	struct page_block block;

	init_page_block(&block, (void *) base, SIZE);

	// NOTE: ADDR(0x1000) should be the first non-bitmap page, and there should only be 7 available pages
	assert(page_block_alloc_single(&block) == (void *) ADDR(1 * PAGE_SIZE));
	assert(page_block_alloc_single(&block) == (void *) ADDR(2 * PAGE_SIZE));
	assert(page_block_alloc_single(&block) == (void *) ADDR(3 * PAGE_SIZE));
	assert(page_block_alloc_single(&block) == (void *) ADDR(4 * PAGE_SIZE));
	page_block_free_single(&block, (page_t *) ADDR(4 * PAGE_SIZE));
	assert(page_block_alloc_single(&block) == (void *) ADDR(4 * PAGE_SIZE));
	assert(page_block_alloc_single(&block) == (void *) ADDR(5 * PAGE_SIZE));
	// Out of memory because it's too big and there's only one page left
	assert(page_block_alloc_single(&block) == (void *) 0);

	page_block_free_single(&block, (page_t *) ADDR(3 * PAGE_SIZE));
	page_block_free_single(&block, (page_t *) ADDR(4 * PAGE_SIZE));
	assert((uint8_t *) page_block_alloc_single(&block) == ADDR(3 * PAGE_SIZE));
	assert((uint8_t *) page_block_alloc_single(&block) == ADDR(4 * PAGE_SIZE));
	assert((uint8_t *) page_block_alloc_single(&block) == 0);

	return 0;
}

int test_page_contiguous_allocation(void)
{
	const size_t SIZE = PAGE_SIZE * 8;

	#define ADDR(addr)	(base + (addr))

	#ifdef HOST_TARGET
	uint8_t *base = malloc(SIZE);
	#else
	uint8_t *base = (uint8_t *) NATIVE_BASE;
	#endif

	struct page_block block;

	init_page_block(&block, (void *) base, SIZE);

	// NOTE: ADDR(0x1000) should be the first non-bitmap page, and there should only be 7 available pages
	assert(page_block_alloc_contiguous(&block, 1 * PAGE_SIZE) == (void *) ADDR(1 * PAGE_SIZE));
	assert(page_block_alloc_contiguous(&block, 2 * PAGE_SIZE) == (void *) ADDR(2 * PAGE_SIZE));
	assert(page_block_alloc_contiguous(&block, 3 * PAGE_SIZE) == (void *) ADDR(4 * PAGE_SIZE));
	// Out of memory because it's too big and there's only one page left
	assert(page_block_alloc_contiguous(&block, 4 * PAGE_SIZE) == (void *) 0);

	// Verify that there's still one page left
	assert(page_block_alloc_contiguous(&block, 1 * PAGE_SIZE) == (void *) ADDR(7 * PAGE_SIZE));

	// Confirm we're now out of memory
	assert(page_block_alloc_contiguous(&block, 1 * PAGE_SIZE) == (void *) 0);

	page_block_free_contiguous(&block, (page_t *) ADDR(3 * PAGE_SIZE), 2 * PAGE_SIZE);
	assert((uint8_t *) page_block_alloc_contiguous(&block, 2 * PAGE_SIZE) == ADDR(3 * PAGE_SIZE));
	assert((uint8_t *) page_block_alloc_contiguous(&block, 2 * PAGE_SIZE) == 0);

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
