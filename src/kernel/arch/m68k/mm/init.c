
#include <errno.h>
#include <kconfig.h>
#include <kernel/printk.h>

#if defined(CONFIG_MMU)
#include <asm/mmu.h>
// TODO this is going to move to mm, so this still makes sense
#include <kernel/proc/memory.h>
#endif

#if defined(CONFIG_MMU)
//mmu_descriptor_t *kernel_mmu_table = NULL;
struct memory_map *kernel_memory_map = NULL;
#endif

void page_fault_handler();

int arch_init_mm(void)
{
	#if defined(CONFIG_MMU)
	kernel_memory_map = memory_map_alloc();
	if (!kernel_memory_map)
		return ENOMEM;

	// Initialize the top-level kernel page table to map directly to real memory
	if (memory_map_mmap(kernel_memory_map, (uintptr_t) 0x00000000, 0x01000000, SEG_FIXED | SEG_SUPERVISOR | SEG_WRITE, NULL, 0) < 0) {
		log_error("error mapping lower memory\n");
		return EFAULT;
	}
	if (memory_map_mmap(kernel_memory_map, (uintptr_t) 0xFF000000, 0x01000000, SEG_FIXED | SEG_NOCACHE | MMU_FLAG_SUPERVISOR | SEG_WRITE, NULL, 0) < 0) {
		log_error("error mapping upper memory\n");
		return EFAULT;
	}

	init_mmu(kernel_memory_map->root_table);
	#endif

	return 0;
}

