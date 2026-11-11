
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
 
#include "../src/kernel/mm/pages.c"
#include "../src/kernel/mm/kmalloc.c"

#define PEEK_AT_KMALLOC_BLOCK_OF(ptr)	(((struct block *) (ptr)) - 1)

int test_kmalloc_alloc_free(void)
{
	int error;
	const size_t SIZE = PAGE_SIZE * 32;

	#ifdef HOST_TARGET
	uint8_t *base = malloc(SIZE);
	#else
	uint8_t *base = (uint8_t *) CONFIG_PAGES_START;
	#endif

	error = init_pages((physical_address_t) base, SIZE, 0);
	assert(error == 0);

	error = init_kernel_heap();
	assert(error == 0);

	const size_t data_size = sizeof(uintptr_t) * 10;
	uintptr_t *data = kmalloc(data_size);
	data[0] = 0;
	data[1] = 1;
	data[2] = 2;
	printf("%lx %lx %lx\n", (uintptr_t) &data[0], (uintptr_t) &data[1], (uintptr_t) &data[2]);
	printf("%lx %ld\n", (uintptr_t) data, PEEK_AT_KMALLOC_BLOCK_OF(data)->size);
	assert(PEEK_AT_KMALLOC_BLOCK_OF(data)->size == data_size + sizeof(struct block));

	uintptr_t *data2 = kmalloc(4);
	printf("%lx %ld\n", (uintptr_t) data2, PEEK_AT_KMALLOC_BLOCK_OF(data2)->size);
	assert(PEEK_AT_KMALLOC_BLOCK_OF(data2)->size == sizeof(uintptr_t) + sizeof(struct block));
	assert((uintptr_t) data2 == ((uintptr_t) data) + PEEK_AT_KMALLOC_BLOCK_OF(data)->size);

	// Test that after freeing the first allocation, its block will be split and reused here
	kmfree(data);
	uintptr_t *data3 = kmalloc(sizeof(uintptr_t) * 2);
	printf("%lx %ld\n", (uintptr_t) data3, PEEK_AT_KMALLOC_BLOCK_OF(data3)->size);
	assert((uintptr_t) data3 == (uintptr_t) data);
	assert(PEEK_AT_KMALLOC_BLOCK_OF(data3)->size == (sizeof(uintptr_t) * 2) + sizeof(struct block));

	// Test that after freeing the 3rd allocation, its block will be recombined, and reused here
	kmfree(data3);
	uintptr_t *data4 = kmalloc(sizeof(uintptr_t) * 10);
	printf("%lx %ld\n", (uintptr_t) data4, PEEK_AT_KMALLOC_BLOCK_OF(data4)->size);
	assert((uintptr_t) data4 == (uintptr_t) data);
	
	//print_free();
	return 0;
}

#define run(test) \
	printf("Running %s\n", #test); \
	assert(test() == 0);

int main(void)
{
	run(test_kmalloc_alloc_free);

	printf("%s tests passed\n", __FILE_NAME__);

	return 0;
}

