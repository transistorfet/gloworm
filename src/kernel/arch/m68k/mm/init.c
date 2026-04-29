
#include <errno.h>
#include <kconfig.h>
#include <kernel/printk.h>
#include <kernel/mm/map.h>

#if defined(CONFIG_MMU)
#include <asm/mmu.h>
#endif

struct memory_map *kernel_memory_map = NULL;

void page_fault_handler();

int arch_init_mm(void)
{
	int error;
	kernel_memory_map = memory_map_alloc();
	if (!kernel_memory_map)
		return ENOMEM;

	// Initialize the top-level kernel page table to map directly to real memory
	error = memory_map_mmap(kernel_memory_map, (uintptr_t) 0x00000000, 0x00700000, SEG_FIXED | SEG_SUPERVISOR | SEG_WRITE, NULL, 0);
	if (error < 0) {
		log_error("error mapping lower memory\n");
		return error;
	}

	error = memory_map_mmap(kernel_memory_map, (uintptr_t) 0x00700000, 0x00100000, SEG_FIXED | SEG_NOCACHE | SEG_SUPERVISOR | SEG_WRITE, NULL, 0);
	if (error < 0) {
		log_error("error mapping local memory mapped I/O\n");
		return error;
	}

	error = memory_map_mmap(kernel_memory_map, (uintptr_t) 0x00800000, 0x00800000, SEG_FIXED | SEG_SUPERVISOR | SEG_WRITE, NULL, 0);
	if (error < 0) {
		log_error("error mapping upper memory\n");
		return error;
	}

	error = memory_map_mmap(kernel_memory_map, (uintptr_t) 0xFF000000, 0x01000000, SEG_FIXED | SEG_NOCACHE | SEG_SUPERVISOR | SEG_WRITE, NULL, 0);
	if (error < 0) {
		log_error("error mapping VME memory space\n");
		return error;
	}

	#if defined(CONFIG_MMU)
	init_mmu(kernel_memory_map->root_table);
	#endif

	return 0;
}

