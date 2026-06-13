
#ifndef _KERNEL_PAGES_H
#define _KERNEL_PAGES_H

#include <stdint.h>
#include <string.h>
#include <kconfig.h>
#include <kernel/utils/queue.h>
#include <asm/addresses.h>

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

#define PAGE_F_NOFAIL		0x8000		/// Always return a page (using pages allocated for emergencies)
#define PAGE_F_SUPERVISOR	0x4000		/// A page used by the kernel itself
#define PAGE_F_TYPE		0x000F		/// Bits that specify the type of page to allocate
#define PAGE_F_LOCAL		0x0001		/// The page must be in local memory (fast access)
#define PAGE_F_SHARED		0x0002		/// The page must be in shared memory (available to all cpus)
#define PAGE_F_USER		0x0000		/// The default settings for user memory


#define ALL_PAGES_AT_INDEX	((uint16_t) 0xffffffff)
#define PAGES_PER_INDEX		16
#define LOG_OF_PAGES_PER_INDEX	4
#define BIT_INDEX(bit)		((bit) >> LOG_OF_PAGES_PER_INDEX)
#define BIT_MASK(bit)		(1 << ((bit) & (PAGES_PER_INDEX - 1)))

typedef uint16_t bitmap_t;
typedef uint8_t page_t[PAGE_SIZE];

struct page_descriptor {
	uint16_t refcount;
};

struct page_domain {
	uint16_t cost[CONFIG_NUM_CPUS];
};

struct page_block {
	struct queue_node node;
	uint32_t flags;
	int total_pages;
	int free_pages;
	physical_address_t base;
	size_t end;
	int bitmap_size;
	bitmap_t *bitmap;
	#if defined(CONFIG_MMU)
	struct page_descriptor *page_descriptors;
	#endif
};

int init_pages(physical_address_t start, size_t size, uint32_t flags);
physical_address_t page_alloc(size_t size, uint32_t flags);
void page_free(physical_address_t ptr, size_t size);
#if defined(CONFIG_MMU)
physical_address_t page_make_ref(physical_address_t ptr, size_t size);
int page_get_ref_single(physical_address_t ptr);
#endif

int init_page_block_with_bitmap(struct page_block *block, size_t size, physical_address_t addr, bitmap_t *bitmap, int bitmap_size, struct page_descriptor *descriptors, uint32_t flags);
int init_page_block(struct page_block *block, physical_address_t addr, size_t size, uint32_t flags);
physical_address_t page_block_alloc(struct page_block *block, size_t size);
void page_block_free(struct page_block *block, physical_address_t ptr, size_t size);
#if defined(CONFIG_MMU)
physical_address_t page_block_make_ref(struct page_block *block, physical_address_t ptr, size_t size);
int page_block_get_ref_single(struct page_block *block, physical_address_t ptr);
#endif

static inline void zero_page(physical_address_t ptr)
{
	memset((void *) ptr, 0, PAGE_SIZE);
}

extern struct page_block pages;

#endif

