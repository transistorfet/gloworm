
#ifndef _ASM_M68K_CONTEXT_H
#define _ASM_M68K_CONTEXT_H

#include <stdint.h>
#include <kconfig.h>
#include <asm/exceptions.h>

#if defined(CONFIG_MMU)
#include <asm/mmu.h>
#endif

struct registers {
	uint32_t d0;
	uint32_t d1;
	uint32_t d2;
	uint32_t d3;
	uint32_t d4;
	uint32_t d5;
	uint32_t d6;
	uint32_t d7;
	uint32_t a0;
	uint32_t a1;
	uint32_t a2;
	uint32_t a3;
	uint32_t a4;
	uint32_t a5;
	uint32_t a6;
	uint32_t usp;
};


struct context {
	struct exception_frame frame;
	struct registers regs;
};

#endif

