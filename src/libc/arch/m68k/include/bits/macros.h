 
#ifndef _ASM_M68K_MACROS_H
#define _ASM_M68K_MACROS_H

#define NOP()			asm volatile("nop\n")
#define GOTO_LABEL(label)	asm volatile("bra " label "\n")
#define STACK_POINTER(ptr)	asm volatile("move.l	%%sp, %0\n" : "=g" ((ptr)))

static inline void inline_delay(int count)
{
	while (--count > 0) { asm volatile(""); }
}

#endif
