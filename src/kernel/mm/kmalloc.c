
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <kconfig.h>
#include <kernel/printk.h>
#include <kernel/mm/pages.h>
#include <kernel/mm/kmalloc.h>
#include <kernel/utils/math.h>


struct block {
	uintptr_t size;
	struct block *next;
};

struct heap {
	struct block *free_blocks;
	size_t total_pages;
	size_t bytes_free;
	size_t bytes_allocated;
};


static struct page_block *kmem_page_block = &pages;
static struct heap main_heap = { 0, 0, 0, 0 };


int init_kernel_heap(void)
{
	main_heap.free_blocks = NULL;
	main_heap.total_pages = 0;
	main_heap.bytes_free = 0;
	main_heap.bytes_allocated = 0;
	return 0;
}

int kernel_heap_set_private_page_block(uintptr_t start, uintptr_t end)
{
	int error;

	if (kmem_page_block != &pages) {
		log_error("kmem: private page block was already set to %x\n", kmem_page_block);
		return -1;
	}

	error = init_page_block(kmem_page_block, (char *) start, end - start);
	if (error < 0) {
		return error;
	}
	return 0;
}

void *kzalloc(uintptr_t size)
{
	void *alloc;
	alloc = kmalloc(size);
	if (!alloc) {
		return NULL;
	}

	memset(alloc, '\0', size);
	return alloc;
}

void *kmalloc(uintptr_t size)
{
	struct block *prev = NULL;
	struct block *nextfree = NULL;
	struct block *cur = main_heap.free_blocks;

	// Align the size to the size of a pointer
	size = roundup(size, sizeof(uintptr_t));
	uintptr_t block_size = size + sizeof(struct block);

	for (; cur; prev = cur, cur = cur->next) {
		if (cur->size >= block_size) {
			// If the block can be split with enough room for another block struct and more than 8 bytes left over, then split it
			if (cur->size >= block_size + sizeof(struct block) + 8) {
				nextfree = (struct block *) ((char *) cur + block_size);
				nextfree->size = cur->size - block_size;
				cur->size = block_size;

				nextfree->next = cur->next;
			} else {
				nextfree = cur->next;
			}
			cur->next = NULL;

			if (prev) {
				prev->next = nextfree;
			} else {
				main_heap.free_blocks = nextfree;
			}

			main_heap.bytes_free -= cur->size;
			main_heap.bytes_allocated += cur->size;
			return (void *) (cur + 1);
		}
	}

	// We're out of free blocks, so attempt to allocate a new page of memory, and panic
	// if we've run out of pages
	uintptr_t page_size = roundup(block_size, PAGE_SIZE);
	cur = (struct block *) page_block_alloc(kmem_page_block, page_size);
	if (!cur) {
		// Out Of Memory
		panic("Kernel out of memory!  Halting...\n");
		return NULL;
	}

	// Successfully allocated a new page.  If it's got enough leftover space to create a new
	// free block, then create one and add it to the free list before we return
	cur->size = page_size;
	cur->next = NULL;
	main_heap.total_pages += page_size;
	main_heap.bytes_allocated += page_size;
	if (page_size - block_size > sizeof(struct block) + 8) {
		cur->size = block_size;
		nextfree = (struct block *) ((char *) cur + block_size);
		nextfree->size = page_size - block_size;
		nextfree->next = NULL;
		// A bit of a hack, but it should work.  Reuse the existing kmfree function to
		// correctly insert the new block into its sorted position in the free list
		kmfree((void *) (nextfree + 1));
	}
	return (void *) (cur + 1);
}

void kmfree(void *ptr)
{
	struct block *prev = NULL;
	struct block *block = ((struct block *) ptr) - 1;

	// Allocated blocks will not be in any list, and `next` is set to NULL during
	// allocation, so if that pointer is not NULL, this block has already been freed
	// or there was some kind of memory corruption that clobbered the block data
	if (block->next) {
		panic("Double free detected at %x! Halting...\n", block);
	}

	main_heap.bytes_free += block->size;
	main_heap.bytes_allocated -= block->size;
	for (struct block *cur = main_heap.free_blocks; cur; prev = cur, cur = cur->next) {
		if (cur->next == block) {
			panic("Double free detected at %x! Halting...\n", cur);
		}

		if ((struct block *) ((char *) cur + cur->size) == block) {
			// Merge the free'd block with the previous block
			cur->size += block->size;

			// If this block is adjacent to the next free block, then merge them
			if ((struct block *) ((char *) cur + cur->size) == cur->next) {
				cur->size += cur->next->size;
				cur->next = cur->next->next;
			}
			return;
		}

		if (cur >= block) {


			// Insert the free'd block into the list
			if (prev) {
				prev->next = block;
			} else {
				main_heap.free_blocks = block;
			}
			block->next = cur;

			// If this block is adjacent to the next free block, then merge them
			if ((struct block *) ((char *) block + block->size) == cur) {
				block->size += cur->size;
				block->next = cur->next;
			}
			return;
		}
	}

	block->next = NULL;
	if (prev) {
		prev->next = block;
	} else if (!main_heap.free_blocks) {
		main_heap.free_blocks = block;
	} else {
		panic("kmalloc free list in broken: couldn't find insertion point but list not empty %x\n", main_heap.free_blocks);
	}
}

void kernel_heap_compact(void)
{
	struct block *prev = NULL;
	struct block *cur = main_heap.free_blocks;

	for (; cur; prev = cur, cur = cur->next) {
		// If this is a page-aligned block that's an even number of pages in size,
		// then free the pages instead of re-inserting it
		if (alignment_offset((uintptr_t) cur, PAGE_SIZE) == 0 && alignment_offset(cur->size, PAGE_SIZE) == 0) {
			if (prev) {
				prev->next = cur->next;
			} else {
				main_heap.free_blocks = cur->next;
			}

			page_block_free(kmem_page_block, (uintptr_t) cur, cur->size >> PAGE_ADDR_BITS);

			main_heap.bytes_free -= cur->size;
			main_heap.total_pages -= cur->size;
		}
	}
}

/*
void print_free(void)
{
	printk_safe("free list:\n");
	for (struct block *cur = main_heap.free_blocks; cur; cur = cur->next) {
		printk_safe("%x: %x\n", cur, cur->size);
	}
}
*/

