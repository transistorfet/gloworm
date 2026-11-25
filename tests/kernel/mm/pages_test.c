
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <kconfig.h>

#include "../src/kernel/mm/pages.c"

#define NATIVE_BASE	0x200000

int test_page_single_allocation(void)
{
	int error;
	const size_t SIZE = PAGE_SIZE * 6;

	#define ADDR(addr)	(base + (addr))

	#ifdef HOST_TARGET
	uint8_t *base = malloc(SIZE);
	#else
	uint8_t *base = (uint8_t *) NATIVE_BASE;
	#endif

	struct page_block block;

	error = init_page_block(&block, (physical_address_t) base, SIZE, 0);
	assert(error == 0);
	assert(block.total_pages == 5);
	assert(block.base == (uintptr_t) ADDR(1 * PAGE_SIZE));

	// NOTE: ADDR(0x1000) should be the first non-bitmap page, and there should only be 7 available pages
	assert(page_block_alloc(&block, PAGE_SIZE) == (physical_address_t) ADDR(1 * PAGE_SIZE));
	assert(page_block_alloc(&block, PAGE_SIZE) == (physical_address_t) ADDR(2 * PAGE_SIZE));
	assert(page_block_alloc(&block, PAGE_SIZE) == (physical_address_t) ADDR(3 * PAGE_SIZE));
	assert(page_block_alloc(&block, PAGE_SIZE) == (physical_address_t) ADDR(4 * PAGE_SIZE));
	page_block_free(&block, (physical_address_t) ADDR(4 * PAGE_SIZE), PAGE_SIZE);
	assert(page_block_alloc(&block, PAGE_SIZE) == (physical_address_t) ADDR(4 * PAGE_SIZE));
	assert(page_block_alloc(&block, PAGE_SIZE) == (physical_address_t) ADDR(5 * PAGE_SIZE));
	// Out of memory because it's too big and there's only one page left
	assert(page_block_alloc(&block, PAGE_SIZE) == (physical_address_t) 0);

	page_block_free(&block, (physical_address_t) ADDR(3 * PAGE_SIZE), PAGE_SIZE);
	page_block_free(&block, (physical_address_t) ADDR(4 * PAGE_SIZE), PAGE_SIZE);
	assert((uint8_t *) page_block_alloc(&block, PAGE_SIZE) == ADDR(3 * PAGE_SIZE));
	assert((uint8_t *) page_block_alloc(&block, PAGE_SIZE) == ADDR(4 * PAGE_SIZE));
	assert((uint8_t *) page_block_alloc(&block, PAGE_SIZE) == 0);

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

	init_page_block(&block, (physical_address_t) base, SIZE, 0);

	// NOTE: ADDR(0x1000) should be the first non-bitmap page, and there should only be 7 available pages
	assert(page_block_alloc(&block, 1 * PAGE_SIZE) == (physical_address_t) ADDR(1 * PAGE_SIZE));
	assert(page_block_alloc(&block, 2 * PAGE_SIZE) == (physical_address_t) ADDR(2 * PAGE_SIZE));
	assert(page_block_alloc(&block, 3 * PAGE_SIZE) == (physical_address_t) ADDR(4 * PAGE_SIZE));
	// Out of memory because it's too big and there's only one page left
	assert(page_block_alloc(&block, 4 * PAGE_SIZE) == (physical_address_t) 0);

	// Verify that there's still one page left
	assert(page_block_alloc(&block, 1 * PAGE_SIZE) == (physical_address_t) ADDR(7 * PAGE_SIZE));

	// Confirm we're now out of memory
	assert(page_block_alloc(&block, 1 * PAGE_SIZE) == (physical_address_t) 0);

	page_block_free(&block, (physical_address_t) ADDR(3 * PAGE_SIZE), 2 * PAGE_SIZE);
	assert((uint8_t *) page_block_alloc(&block, 2 * PAGE_SIZE) == ADDR(3 * PAGE_SIZE));
	assert((uint8_t *) page_block_alloc(&block, 2 * PAGE_SIZE) == 0);

	return 0;
}

int test_page_block_initialization(void)
{
	int error;

	#define ADDR(addr)	(base + (addr))

	#ifdef HOST_TARGET
	uint8_t *base = malloc(PAGE_SIZE * (PAGE_SIZE << 3));
	#else
	uint8_t *base = (uint8_t *) NATIVE_BASE;
	#endif

	size_t total_pages;
	struct page_block block;

	total_pages = PAGE_SIZE >> 3;
	error = init_page_block(&block, (physical_address_t) base, total_pages * PAGE_SIZE, 0);
	assert(error == 0);
	assert(block.total_pages == total_pages - 1);
	assert(block.base == (physical_address_t) ADDR(1 * PAGE_SIZE));

	total_pages = PAGE_SIZE << 3;
	error = init_page_block(&block, (physical_address_t) base, total_pages * PAGE_SIZE, 0);
	assert(error == 0);
	assert(block.total_pages == total_pages - 17);
	assert(block.base == (physical_address_t) ADDR(17 * PAGE_SIZE));

	return 0;
}

#define run(test) \
	printf("Running %s\n", #test); \
	assert(test() == 0);

int main()
{
	run(test_page_single_allocation);
	run(test_page_contiguous_allocation);
	run(test_page_block_initialization);

	printf("%s tests passed\n", __FILE_NAME__);

	return 0;
}
