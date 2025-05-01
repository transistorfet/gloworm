
#ifndef _KERNEL_PAGES_H
#define _KERNEL_PAGES_H

#include <stdint.h>
#include <kconfig.h>
#include <kernel/utils/queue.h>

#if defined(CONFIG_PAGE_SIZE_256)
#define PAGE_ADDR_BITS	8
#elif defined(CONFIG_PAGE_SIZE_512)
#define PAGE_ADDR_BITS	9
#elif defined(CONFIG_PAGE_SIZE_1K)
#define PAGE_ADDR_BITS	10
#elif defined(CONFIG_PAGE_SIZE_2K)
#define PAGE_ADDR_BITS	11
#elif defined(CONFIG_PAGE_SIZE_4K)
#define PAGE_ADDR_BITS	12
#elif defined(CONFIG_PAGE_SIZE_8K)
#define PAGE_ADDR_BITS	13
#elif defined(CONFIG_PAGE_SIZE_16K)
#define PAGE_ADDR_BITS	14
#elif defined(CONFIG_PAGE_SIZE_32K)
#define PAGE_ADDR_BITS	15
#elif defined(CONFIG_PAGE_SIZE_64K)
#define PAGE_ADDR_BITS	16
#endif

#define PAGE_SIZE	(1 << PAGE_ADDR_BITS)

#define PAGE_TYPE	0x0007		/// Bits that specify the type of page to allocate
#define PAGE_KERNEL	0x0001		/// A page used by the kernel itself
#define PAGE_USER	0x0002		/// A page used by a user process
#define PAGE_LOCAL	0x0010		/// The page must be in local memory (fast access)
#define PAGE_NOFAIL	0x8000		/// Always return a page (using pages allocated for emergencies)

char *page_alloc(int pages);
void page_free(char *ptr);


#define ALL_PAGES_AT_INDEX	((uint8_t) 0xffffffff)
#define PAGES_PER_INDEX		8
#define LOG_OF_PAGES_PER_INDEX	3
#define BIT_INDEX(bit)		((bit) >> LOG_OF_PAGES_PER_INDEX)
#define BIT_MASK(bit)		(1 << ((bit) & (PAGES_PER_INDEX - 1)))

typedef uint8_t bitmap_t;

typedef uint8_t page_t[PAGE_SIZE];

struct page_domain {
	uint16_t cost[CONFIG_NUM_CPUS];
};

struct page_block {
	struct queue_node node;
	bitmap_t *bitmap;
	int bitmap_size;
	page_t *base;
	int pages;
};


page_t *page_block_alloc_single(struct page_block *block);
void page_block_free_single(struct page_block *block, page_t *ptr);
void page_block_free_contiguous(struct page_block *block, page_t *ptr, size_t size);
page_t *page_block_alloc_contiguous(struct page_block *block, int contiguous_pages);

#endif

