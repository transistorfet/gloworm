
#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <kconfig.h>
#include <kernel/printk.h>
#include <kernel/mm/pages.h>
#include <kernel/utils/macros.h>

#define CONFIG_MAX_PAGE_BLOCKS	4

static int num_global_blocks;
static struct page_block global_blocks[CONFIG_MAX_PAGE_BLOCKS];

int init_page_block_with_bitmap(struct page_block *block, size_t size, physical_address_t addr, bitmap_t *bitmap, int bitmap_size, struct page_descriptor *descriptors, uint32_t flags)
{
	_queue_node_init(&block->node);
	block->flags = flags;
	block->total_pages = size >> PAGE_ADDR_BITS;
	block->free_pages = block->total_pages;
	block->base = addr;
	block->bitmap = bitmap;
	block->bitmap_size = bitmap_size;
	memset(block->bitmap, '\0', block->bitmap_size);

	#if defined(CONFIG_MMU)
	block->page_descriptors = descriptors;
	memset(block->page_descriptors, '\0', block->total_pages);
	#endif

	log_trace("pages %x to %x, %d pages, %d bitmap size\n", addr, addr + size, block->total_pages, block->bitmap_size);
	// Check that the bitmap is big enough, and return an error if it's too small
	if ((bitmap_size << LOG_OF_PAGES_PER_INDEX) < block->total_pages) {
		return -1;
	}

	return 0;
}

int init_page_block(struct page_block *block, physical_address_t addr, size_t size, uint32_t flags)
{
	physical_address_t pages_addr;
	int descriptors_pages = 0;
	int total_pages, allocatable_pages;
	int bitmap_size, bitmap_pages;
	void *descriptors_addr = NULL;
	#if defined(CONFIG_MMU)
	int descriptors_size = 0;
	#endif

	total_pages = size >> PAGE_ADDR_BITS;

	bitmap_size = align_up(total_pages >> LOG_OF_PAGES_PER_INDEX, PAGES_PER_INDEX);
	bitmap_pages = align_up(bitmap_size, PAGE_SIZE) >> PAGE_ADDR_BITS;

	#if defined(CONFIG_MMU)
	descriptors_size = (total_pages - bitmap_pages) * sizeof(struct page_descriptor);
	descriptors_pages = align_up(descriptors_size, PAGE_SIZE) >> PAGE_ADDR_BITS;
	descriptors_addr = &((page_t *) addr)[bitmap_pages];
	#endif

	allocatable_pages = total_pages - bitmap_pages - descriptors_pages;
	pages_addr = (physical_address_t) &((page_t *) addr)[bitmap_pages + descriptors_pages];

	log_trace("bitmaps at %x, size %d, pages %d, after %x\n", addr, bitmap_size, bitmap_pages, pages_addr);
	#if defined(CONFIG_MMU)
	log_trace("descriptors at %x, size %d, pages %d, after %x\n", descriptors_addr, descriptors_size, descriptors_pages, pages_addr);
	#endif
	return init_page_block_with_bitmap(block, allocatable_pages << PAGE_ADDR_BITS, pages_addr, (bitmap_t *) addr, bitmap_size, descriptors_addr, flags);
}

int append_global_page_block(physical_address_t start, size_t size, uint32_t flags)
{
	if (num_global_blocks > 0) {
		const size_t previous_last_block_end = global_blocks[num_global_blocks].base + (global_blocks[num_global_blocks].total_pages * PAGE_SIZE);
		if (start < previous_last_block_end) {
			log_debug("pages: overlapping page block (note: they must be added in ascending order)\n");
			return EINVAL;
		}
	}

	num_global_blocks += 1;
	return init_page_block(&global_blocks[num_global_blocks - 1], start, size, flags);
}

int init_pages(physical_address_t start, size_t size, uint32_t flags)
{
	int error;

	num_global_blocks = 0;

	error = append_global_page_block(start, size, flags);
	if (error < 0)
		return error;

	return 0;
}

physical_address_t page_alloc(size_t size, uint32_t flags)
{
	const int contiguous_pages = size >> PAGE_ADDR_BITS;

	for (int i = 0; i < num_global_blocks; i++) {
		if ((global_blocks[i].flags & (PAGE_F_TYPE & flags)) == (PAGE_F_TYPE & flags) && global_blocks[i].free_pages > contiguous_pages) {
			return page_block_alloc(&global_blocks[i], size);
		}
	}
	return NULL;
}

void page_free(physical_address_t ptr, size_t size)
{
	for (int i = 0; i < num_global_blocks; i++) {
		if (ptr >= global_blocks[i].base && ptr <= global_blocks[i].base + (global_blocks[i].total_pages * PAGE_SIZE)) {
			page_block_free(&global_blocks[i], ptr, size);
			return;
		}
	}
}

#if defined(CONFIG_MMU)

physical_address_t page_make_ref(physical_address_t ptr, size_t size)
{
	for (int i = 0; i < num_global_blocks; i++) {
		if (ptr >= global_blocks[i].base && ptr <= global_blocks[i].base + (global_blocks[i].total_pages * PAGE_SIZE)) {
			page_block_make_ref(&global_blocks[i], ptr, size);
		}
	}
	return NULL;
}

int page_get_ref_single(physical_address_t ptr)
{
	for (int i = 0; i < num_global_blocks; i++) {
		if (ptr >= global_blocks[i].base && ptr <= global_blocks[i].base + (global_blocks[i].total_pages * PAGE_SIZE)) {
			page_block_get_ref_single(&global_blocks[i], ptr);
		}
	}
	return 0;
}

#endif

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
	while (start_bit < block->total_pages) {
		if (bitmap[BIT_INDEX(start_bit)] == ALL_PAGES_AT_INDEX) {
			// If all bits are allocated, move to the next index
			start_bit += PAGES_PER_INDEX;
		} else if ((bitmap[BIT_INDEX(start_bit)] & BIT_MASK(start_bit)) != 0) {
			// If this single bit is already allocated, then move to the next bit
			start_bit++;
		} else {
			// Attempt to find `contiguous_pages` free bits, or restart at the next `start_bit`
			for (end_bit = start_bit + 1; end_bit < block->total_pages && end_bit - start_bit < contiguous_pages; end_bit++) {
				// Break early if we didn't find enough bits
				if ((bitmap[BIT_INDEX(end_bit)] & BIT_MASK(end_bit)) != 0) {
					break;
				}
			}

			// If not enough pages were found, then restart at the next start bit
			if (end_bit > block->total_pages || end_bit - start_bit < contiguous_pages) {
				start_bit++;
				continue;
			}

			// Go through each bit between start and end and set it to 1, and set the refcount to 1
			for (bit = start_bit; bit < end_bit; bit++) {
				#if defined(CONFIG_MMU)
				block->page_descriptors[bit].refcount = 1;
				log_trace("allocated page %x\n", &((page_t *) block->base)[bit]);
				#endif
				bitmap[BIT_INDEX(bit)] |= BIT_MASK(bit);
			}
			block->free_pages -= contiguous_pages;

			return (physical_address_t) &((page_t *) block->base)[start_bit];
		}
	}

	return NULL;
}

void page_block_free(struct page_block *block, physical_address_t ptr, size_t size)
{
	int page_number = (((uintptr_t) ptr) - ((uintptr_t) block->base)) >> PAGE_ADDR_BITS;

	for (; size >= PAGE_SIZE; page_number++, size -= PAGE_SIZE) {
		#if defined(CONFIG_MMU)
		log_trace("decrement refcount for %x to %d\n", &((page_t *) block->base)[page_number], block->page_descriptors[page_number].refcount - 1);
		if (--block->page_descriptors[page_number].refcount != 0) {
			continue;
		}
		#endif

		int index = BIT_INDEX(page_number);
		block->bitmap[index] &= ~(BIT_MASK(page_number));
		block->free_pages += 1;
	}
}

#if defined(CONFIG_MMU)
physical_address_t page_block_make_ref(struct page_block *block, physical_address_t ptr, size_t size)
{
	int page_number = (((uintptr_t) ptr) - ((uintptr_t) block->base)) >> PAGE_ADDR_BITS;

	for (; size >= PAGE_SIZE; page_number++, size -= PAGE_SIZE) {
		block->page_descriptors[page_number].refcount++;
		log_trace("increment refcount for %x to %d\n", &((page_t *) block->base)[page_number], block->page_descriptors[page_number].refcount);
	}
	return ptr;
}

int page_block_get_ref_single(struct page_block *block, physical_address_t ptr)
{
	int page_number = (((uintptr_t) ptr) - ((uintptr_t) block->base)) >> PAGE_ADDR_BITS;

	return block->page_descriptors[page_number].refcount;
}
#endif

