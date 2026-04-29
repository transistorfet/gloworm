
#include <stdint.h>
#include <string.h>
#include <sys/param.h>

#include <kconfig.h>
#include <kernel/mm/pages.h>
#include <kernel/printk.h>

struct page_block pages = { 0 };


int init_page_block_with_bitmap(struct page_block *block, bitmap_t *bitmap, int bitmap_size, void *addr, size_t size, struct page_descriptor *descriptors)
{
	_queue_node_init(&block->node);
	block->bitmap = bitmap;
	block->bitmap_size = bitmap_size;
	block->base = addr;
	block->pages = size >> PAGE_ADDR_BITS;
	memset(block->bitmap, '\0', block->bitmap_size);

	#if defined(CONFIG_MMU)
	block->page_descriptors = descriptors;
	memset(block->page_descriptors, '\0', block->pages);
	#endif

	log_trace("pages %x to %x, %d pages, %d bitmap size\n", addr, addr + size, block->pages, block->bitmap_size);
	// Check that the bitmap is big enough, and return an error if it's too small
	if ((bitmap_size << LOG_OF_PAGES_PER_INDEX) < block->pages) {
		return -1;
	}

	return 0;
}

int init_page_block(struct page_block *block, void *addr, size_t size)
{
	void *pages_addr;
	int descriptors_pages = 0;
	int total_pages, allocatable_pages;
	int bitmap_size, bitmap_pages;
	void *descriptors_addr = NULL;
	#if defined(CONFIG_MMU)
	int descriptors_size = 0;
	#endif

	total_pages = size >> PAGE_ADDR_BITS;

	bitmap_size = roundup(total_pages >> LOG_OF_PAGES_PER_INDEX, PAGES_PER_INDEX);
	bitmap_pages = roundup(bitmap_size, PAGE_SIZE) >> PAGE_ADDR_BITS;

	#if defined(CONFIG_MMU)
	descriptors_size = (total_pages - bitmap_pages) * sizeof(struct page_descriptor);
	descriptors_pages = roundup(descriptors_size, PAGE_SIZE) >> PAGE_ADDR_BITS;
	descriptors_addr = &((page_t *) addr)[bitmap_pages];
	#endif

	allocatable_pages = total_pages - bitmap_pages - descriptors_pages;
	pages_addr = &((page_t *) addr)[bitmap_pages + descriptors_pages];

	log_trace("bitmaps at %x, size %d, pages %d, after %x\n", addr, bitmap_size, bitmap_pages, pages_addr);
	#if defined(CONFIG_MMU)
	log_trace("descriptors at %x, size %d, pages %d, after %x\n", descriptors_addr, descriptors_size, descriptors_pages, pages_addr);
	#endif
	return init_page_block_with_bitmap(block, (bitmap_t *) addr, bitmap_size, pages_addr, allocatable_pages << PAGE_ADDR_BITS, descriptors_addr);
}

/// Allocate one or more contiguous pages from the given block of memory
///
/// Returns a pointer to the first page, or NULL if not found.
/// It doesn't record the length of the allocation, so when freeing pages, the
/// allocating code must keep track of the size and deallocate each page
/// separately
physical_address_t page_block_alloc(struct page_block *block, size_t size)
{
	int contiguous_pages = size >> PAGE_ADDR_BITS;
	uint32_t bit, start_bit, end_bit;
	bitmap_t *bitmap = block->bitmap;

	start_bit = 0;
	while (start_bit < block->pages) {
		if (bitmap[BIT_INDEX(start_bit)] == ALL_PAGES_AT_INDEX) {
			// If all bits are allocated, move to the next index
			start_bit += PAGES_PER_INDEX;
		} else if ((bitmap[BIT_INDEX(start_bit)] & BIT_MASK(start_bit)) != 0) {
			// If this single bit is already allocated, then move to the next bit
			start_bit++;
		} else {
			// Attempt to find `contiguous_pages` free bits, or restart at the next `start_bit`
			for (end_bit = start_bit + 1; end_bit < block->pages && end_bit - start_bit < contiguous_pages; end_bit++) {
				// Break early if we didn't find enough bits
				if ((bitmap[BIT_INDEX(end_bit)] & BIT_MASK(end_bit)) != 0) {
					break;
				}
			}

			// If not enough pages were found, then restart at the next start bit
			if (end_bit > block->pages || end_bit - start_bit < contiguous_pages) {
				start_bit++;
				continue;
			}

			// Go through each bit between start and end and set it to 1, and set the refcount to 1
			for (bit = start_bit; bit < end_bit; bit++) {
				#if defined(CONFIG_MMU)
				block->page_descriptors[bit].refcount = 1;
				log_trace("allocated page %x\n", &block->base[bit]);
				#endif
				bitmap[BIT_INDEX(bit)] |= BIT_MASK(bit);
			}

			return (physical_address_t) &block->base[start_bit];
		}
	}

	return NULL;
}

void page_block_free(struct page_block *block, physical_address_t ptr, size_t size)
{
	int page_number = (((uintptr_t) ptr) - ((uintptr_t) block->base)) >> PAGE_ADDR_BITS;

	for (; size >= PAGE_SIZE; page_number++, size -= PAGE_SIZE) {
		#if defined(CONFIG_MMU)
		log_trace("decrement refcount for %x to %d\n", &block->base[page_number], block->page_descriptors[page_number].refcount - 1);
		if (--block->page_descriptors[page_number].refcount != 0) {
			continue;
		}
		#endif

		int index = BIT_INDEX(page_number);
		block->bitmap[index] &= ~(BIT_MASK(page_number));
	}
}

#if defined(CONFIG_MMU)
physical_address_t page_block_make_ref(struct page_block *block, physical_address_t ptr, size_t size)
{
	int page_number = (((uintptr_t) ptr) - ((uintptr_t) block->base)) >> PAGE_ADDR_BITS;

	for (; size >= PAGE_SIZE; page_number++, size -= PAGE_SIZE) {
		block->page_descriptors[page_number].refcount++;
		log_trace("increment refcount for %x to %d\n", &block->base[page_number], block->page_descriptors[page_number].refcount);
	}
	return ptr;
}

int page_block_get_ref_single(struct page_block *block, physical_address_t ptr)
{
	int page_number = (((uintptr_t) ptr) - ((uintptr_t) block->base)) >> PAGE_ADDR_BITS;

	return block->page_descriptors[page_number].refcount;
}
#endif

