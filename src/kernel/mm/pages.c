
#include <stdint.h>

#include <kconfig.h>
#include <kernel/mm/pages.h>


int init_page_block_with_bitmap(struct page_block *block, bitmap_t *bitmap, int bitmap_size, void *addr, int size)
{
	_queue_node_init(&block->node);
	block->bitmap = bitmap;
	block->bitmap_size = bitmap_size;
	block->base = addr;
	block->pages = size >> PAGE_ADDR_BITS;

	// Check that the bitmap is big enough, and return an error if it's too small
	if ((bitmap_size << LOG_OF_PAGES_PER_INDEX) < block->pages) {
		return -1;
	}

	for (int pos = 0; pos < block->bitmap_size; pos++) {
		block->bitmap[pos] = 0;
	}

	return 0;
}

int init_page_block(struct page_block *block, void *addr, int size)
{
	int pages;
	int bitmap_size;
	int bitmap_pages;

	pages = size >> PAGE_ADDR_BITS;

	bitmap_size = pages >> LOG_OF_PAGES_PER_INDEX;
	if ((pages & (PAGES_PER_INDEX - 1)) != 0)
		bitmap_size++;

	bitmap_pages = bitmap_size >> PAGE_ADDR_BITS;
	if ((bitmap_size & (PAGE_SIZE - 1)) != 0)
		bitmap_pages++;

	return init_page_block_with_bitmap(block, (bitmap_t *) addr, bitmap_size, &((page_t *) addr)[bitmap_pages], size - (bitmap_pages << PAGE_ADDR_BITS));
}

// TODO should this function only be used when there's no MMU and contiguous blocks are required
//#if !defined(CONFIG_MMU)

/// Allocate 1 or more contiguous pages from the given block of memory
///
/// Returns a pointer to the first page, or NULL if not found.
/// It doesn't record the length of the allocation, so when freeing pages, the
/// allocating code must keep track of the size and deallocate each page
/// separately
page_t *page_block_alloc_contiguous(struct page_block *block, int contiguous_pages)
{
	uint32_t bit, start_bit, end_bit;
	bitmap_t *bitmap = block->bitmap;

	for (start_bit = 0; start_bit < block->pages; ) {
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

			// Go through each bit between start and end and set it to 1
			bit = start_bit;
			while (bit < end_bit) {
				if (bit >> PAGES_PER_INDEX != 0) {
					bitmap[BIT_INDEX(bit)] = ALL_PAGES_AT_INDEX;
					bit += PAGES_PER_INDEX;
				} else {
					bitmap[BIT_INDEX(bit)] |= BIT_MASK(bit);
					bit += 1;
				}
			} 

			//return block->base + ((start_index * PAGES_PER_INDEX + start_bit) << PAGE_ADDR_BITS);
			return &block->base[start_bit];
		}
	}

	return NULL;
}
//#endif

/// Allocate a single page and return a pointer, or NULL if out of memory
page_t *page_block_alloc_single(struct page_block *block)
{
	uint32_t bit;
	bitmap_t *bitmap = block->bitmap;

	bit = 0;
	while (bit < block->pages) {
		if (bitmap[BIT_INDEX(bit)] == ALL_PAGES_AT_INDEX) {
			bit += PAGES_PER_INDEX;
		} else if ((bitmap[BIT_INDEX(bit)] & BIT_MASK(bit)) != 0) {
			bit += 1;
		} else {
			bitmap[BIT_INDEX(bit)] |= BIT_MASK(bit);
			return &block->base[bit];
		}
	}
	return NULL;
}

/// Free a single page of memory, given the address of the page (as returned by alloc)
void page_block_free_single(struct page_block *block, page_t *ptr)
{
	int page_number = (((uintptr_t) ptr) - ((uintptr_t) block->base)) >> PAGE_ADDR_BITS;

	int index = BIT_INDEX(page_number);
	block->bitmap[index] &= ~(BIT_MASK(page_number));
}

void page_block_free_contiguous(struct page_block *block, page_t *ptr, size_t size)
{
	int page_number = (((uintptr_t) ptr) - ((uintptr_t) block->base)) >> PAGE_ADDR_BITS;

	for (; size >= PAGE_SIZE; page_number++, size -= PAGE_SIZE) {
		int index = BIT_INDEX(page_number);
		block->bitmap[index] &= ~(BIT_MASK(page_number));
	}
}

