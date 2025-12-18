
#include <kconfig.h>

#if defined(CONFIG_MMU)
#include <asm/mmu.h>
#endif

void page_fault_handler();

int arch_init_mm(void)
{
	#if defined(CONFIG_MMU)
	init_mmu();
	#endif

	return 0;
}

