
#ifndef _ASM_M68K_IRQS_H
#define _ASM_M68K_IRQS_H

#include <kernel/interrupts.h>

// 68k Interrupt Vectors Numbers
#define IRQ_BUS_ERROR		2
#define IRQ_ADDRESS_ERROR	3
#define IRQ_ILLEGAL_INSTRUCTION	4
#define IRQ_ZERO_DIVIDE		5
#define IRQ_CHK_INSTRUCTION	6
#define IRQ_TRAPV_INSTRUCTION	7
#define IRQ_PRIVILEGE_VIOLATION	8
#define IRQ_TRACE		9
#define IRQ_LINE_1010_EMULATOR	10
#define IRQ_LINE_1111_EMULATOR	11

#define IRQ_FORMAT_ERROR	14
#define IRQ_UNINITIALIZED_VEC	15

#define IRQ_TRAP0		32
#define IRQ_TRAP1		33
#define IRQ_TRAP2		34
#define IRQ_TRAP3		35
#define IRQ_TRAP4		36
#define IRQ_TRAP5		37
#define IRQ_TRAP6		38
#define IRQ_TRAP7		39
#define IRQ_TRAP8		40
#define IRQ_TRAP9		41
#define IRQ_TRAP10		42
#define IRQ_TRAP11		43
#define IRQ_TRAP12		44
#define IRQ_TRAP13		45
#define IRQ_TRAP14		46
#define IRQ_TRAP15		47

#define IRQ_USER_MAX		64


/*** Macros ***/

#define HALT()			asm volatile("stop #0x2700\n")

#define DISABLE_INTS()		asm volatile("or.w	#0x0700, %sr");
#define ENABLE_INTS()		asm volatile("and.w	#0xF8FF, %sr");

#define TRACE_ON()		asm volatile("or.w	#0x8000, %sr");
#define TRACE_OFF()		asm volatile("and.w	#0x7FFF, %sr");

#define ARDUINO_TRACE_ON()	asm volatile("movea.l	#0x2019, %%a0\n" "move.b	#1, (%%a0)" : : : "%a0");
#define ARDUINO_TRACE_OFF()	asm volatile("movea.l	#0x2019, %%a0\n" "move.b	#0, (%%a0)" : : : "%a0");

typedef short lock_state_t;

#define LOCK(saved) {					\
	asm("move.w	%%sr, %0\n" : "=dm" ((saved)));	\
	DISABLE_INTS();					\
}

#define UNLOCK(saved) {						\
	asm("move.w	%0, %%sr\n" : : "dm" ((saved)) :);	\
}

typedef void (*irq_handler_t)();

void init_irqs();
void set_irq_handler(irq_num_t irq, irq_handler_t handler);

void do_irq(int irq);

#endif

