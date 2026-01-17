
#include <kconfig.h>
#include <kernel/printk.h>
#include <kernel/proc/memory.h>
#include <kernel/proc/process.h>

#if defined(CONFIG_MMU)

#include <asm/mmu.h>
#include <asm/addresses.h>

#define READ		0
#define WRITE		1


static int fetch_page(struct memory_map *map, virtual_address_t page_virt_addr, char write_flag, char **result)
{
	int error;
	char *page;

	page = (char *) mmu_table_get_page(map->root_table, page_virt_addr);
	if (!page) {
		// TODO this doesn't return the page.  I can look it up after and fault a second
		// time, but this just seems a bit messy now.  Should I make load page optionally
		// set a pointer with the page?  What kind of error should these functions return?
		// how would they normally be detected?  Should this instead do something drastic,
		// like kill the process with a signal?  It can't given only the map is passed in,
		// so it either needs the process, or it needs to return an error that can be
		// detected.  I guess even with a signal it would need to pass back an error of some
		// kind... Maybe I need to pass back a negative number, and always check for negative
		// similar to how read/write syscalls work?
		error = memory_map_load_page_at(map, page_virt_addr, write_flag);
		if (error < 0) {
			return error;
		}

		page = (char *) mmu_table_get_page(map->root_table, page_virt_addr);
		if (!page) {
			return ENOENT;
		}
	}
	*result = page;
	return 0;
}

int generic_memcpy_from_user_map(struct memory_map *map, void *dest, virtual_address_t src, size_t size)
{
	int error;
	char *page;
	size_t i = 0;
	size_t chunk_size;
	virtual_address_t page_virt_addr, page_offset;

	page_virt_addr = src & ~(PAGE_SIZE - 1);
	page_offset = src & (PAGE_SIZE - 1);
	while (1) {
/*
		page = (char *) mmu_table_get_page(map->root_table, page_virt_addr);
		if (!page) {
			// TODO this doesn't return the page.  I can look it up after and fault a second
			// time, but this just seems a bit messy now.  Should I make load page optionally
			// set a pointer with the page?  What kind of error should these functions return?
			// how would they normally be detected?  Should this instead do something drastic,
			// like kill the process with a signal?  It can't given only the map is passed in,
			// so it either needs the process, or it needs to return an error that can be
			// detected.  I guess even with a signal it would need to pass back an error of some
			// kind... Maybe I need to pass back a negative number, and always check for negative
			// similar to how read/write syscalls work?
			if (memory_map_load_page_at(map, page_virt_addr, 0) < 0) {
				return 0;
			}

			page = (char *) mmu_table_get_page(map->root_table, page_virt_addr);
			if (!page) {
				return 0;
			}
		}
*/

		error = fetch_page(map, page_virt_addr, READ, &page);
		if (error < 0) {
			return error;
		}

		chunk_size = (size - i < PAGE_SIZE - page_offset) ? size - i : PAGE_SIZE - page_offset;
		memcpy(&((char *) dest)[i], &page[page_offset], chunk_size);

		i += chunk_size;
		if (i >= size) {
			return i;
		}

		page_offset += chunk_size;
		if (page_offset >= PAGE_SIZE) {
			page_virt_addr += PAGE_SIZE;
			page_offset = 0;
		}
	}
}

int generic_memcpy_to_user_map(struct memory_map *map, virtual_address_t dest, const void *src, size_t size)
{
	int error;
	char *page;
	size_t i = 0;
	size_t chunk_size;
	virtual_address_t page_virt_addr, page_offset;

	page_virt_addr = dest & ~(PAGE_SIZE - 1);
	page_offset = dest & (PAGE_SIZE - 1);
	while (1) {
/*
		page = (char *) mmu_table_get_page(map->root_table, page_virt_addr);

		if (!page) {
// TODO maybe m'ke this use a memory map function that either fetches the page
// or uses the segment flags and memory ops to load one
// maybe this courd be used for all 4 functions, since it could be copying from
// unloaded data pages in the user process, and these functions are specifically
// for when you can't rely on page faults to load things since proc != current_proc
			return i;
		}
*/
		error = fetch_page(map, page_virt_addr, WRITE, &page);
		if (error < 0) {
			return error;
		}

		chunk_size = (size - i < PAGE_SIZE - page_offset) ? size - i : PAGE_SIZE - page_offset;
		memcpy(&page[page_offset], &((char *) src)[i], chunk_size);

		i += chunk_size;
		if (i >= size) {
			return i;
		}

		page_offset += chunk_size;
		if (page_offset >= PAGE_SIZE) {
			page_virt_addr += PAGE_SIZE;
			page_offset = 0;
		}
	}
}

int generic_strncpy_from_user_map(struct memory_map *map, char *dest, virtual_address_t src, size_t max)
{
	int error;
	char *page;
	size_t i = 0;
	size_t chunk_size;
	virtual_address_t page_virt_addr, page_offset;

	page_virt_addr = src & ~(PAGE_SIZE - 1);
	page_offset = src & (PAGE_SIZE - 1);
	while (1) {
		error = fetch_page(map, page_virt_addr, READ, &page);
		if (error < 0) {
			return error;
		}

		chunk_size = (max - i < PAGE_SIZE - page_offset) ? max - i : PAGE_SIZE - page_offset;
		for (; chunk_size > 0 && page[page_offset] != '\0'; chunk_size--) {
			dest[i++] = page[page_offset++];
		}

		if (chunk_size > 0 || i >= max) {
			dest[i] = '\0';
			return i;
		}

		if (page_offset >= PAGE_SIZE) {
			page_virt_addr += PAGE_SIZE;
			page_offset = 0;
		}
	}
}

int generic_strncpy_to_user_map(struct memory_map *map, virtual_address_t dest, const char *src, size_t max)
{
	int error;
	char *page;
	size_t i = 0;
	size_t chunk_size;
	virtual_address_t page_virt_addr, page_offset;

	page_virt_addr = dest & ~(PAGE_SIZE - 1);
	page_offset = dest & (PAGE_SIZE - 1);
	while (1) {
/*
		page = (char *) mmu_table_get_page(map->root_table, page_virt_addr);
		if (!page) {
			return i;
		}
*/

		error = fetch_page(map, page_virt_addr, WRITE, &page);
		if (error < 0) {
			return error;
		}

		chunk_size = (max - i < PAGE_SIZE - page_offset) ? max - i : PAGE_SIZE - page_offset;
		for (; src[i] != '\0' && chunk_size > 0; chunk_size--) {
			page[page_offset++] = src[i++];
		}

		if (chunk_size > 0 || i >= max) {
			page[page_offset] = '\0';
			return i;
		}

		if (page_offset >= PAGE_SIZE) {
			page_virt_addr += PAGE_SIZE;
			page_offset = 0;
		}
	}
}
#endif

