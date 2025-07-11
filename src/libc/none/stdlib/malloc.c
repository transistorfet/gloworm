
#include <stdio.h>
#include <stddef.h>

struct block {
	int size;
	struct block *next;
};

struct heap {
	struct block *free_blocks;
};


static struct heap main_heap = { 0 };


void init_heap(void *addr, unsigned long size)
{
	struct block *space = (struct block *) addr;

	space->size = size - sizeof(struct block);
	space->next = NULL;

	main_heap.free_blocks = space;
}

void *malloc(unsigned long size)
{
	struct block *prev = NULL;
	struct block *nextfree = NULL;
	struct block *cur = main_heap.free_blocks;

	// Align the size to 4 bytes
	size += ((4 - (size & 0x3)) & 0x3);
	int block_size = size + sizeof(struct block);

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

			return (void *) (cur + 1);
		}
	}
	// Out Of Memory
	printf("Out of memory!\n");
	return NULL;
}

void free(void *ptr)
{
	struct block *prev = NULL;
	struct block *block = ((struct block *) ptr) - 1;

	for (struct block *cur = main_heap.free_blocks; cur; prev = cur, cur = cur->next) {
		if (cur->next == block) {
			printf("Double free detected at %x! Halting...\n", (unsigned int) cur);
			return;
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
}

/*
void print_free()
{
	int i = 0;
	struct block *cur = main_heap.free_blocks;
	for (; cur; cur = cur->next, i++) {
		printf("%d %x %x\n", i, cur, cur->size);
	} 
}
*/
