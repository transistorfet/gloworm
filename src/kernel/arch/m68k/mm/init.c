
#include <kconfig.h>

#if defined(CONFIG_MMU)
#include <asm/mmu.h>
#endif

int arch_init_mm(void)
{
	#if defined(CONFIG_MMU)
	init_mmu();
	#endif

	return 0;
}

