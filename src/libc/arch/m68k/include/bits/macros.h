 
#ifndef _ASM_M68K_MACROS_H
#define _ASM_M68K_MACROS_H

#define NOP()			asm volatile("nop\n")
#define GOTO_LABEL(label)	asm volatile("bra " label "\n")

static inline void inline_delay(int count)
{
	while (--count > 0) { asm volatile(""); }
}

#endif
