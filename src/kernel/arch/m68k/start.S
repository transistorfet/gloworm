
	#include <kconfig.h>

	.global _start
	.global	main
	.global boot_args
	.global __bss_start
	.global __bss_end
	.global __boot_stack_top

/**
 * Image Start
 */

.section    .text.start
_start:
	/* Disable interrupts */
	move.w  #0x2700, %sr

	/* Zero out the BSS section of the kernel */
	lea.l	__bss_start, %a0
	move.l	#__bss_end, %d0
	sub.l	%a0, %d0
	lsr.l	#2, %d0
    5:	clr.l	(%a0)+
	dbf	%d0, 5b

	/* Save boot args from the old stack before re-initializing */
	movea.l	(+4,%sp), %a0
	lea	boot_args, %a1
    10:	move.b	(%a0)+, (%a1)+
	bne	10b

	/* Re-initialize the stack pointer to the top of the kernel space */
	move.l	#__boot_stack_top, %sp

	/* Set a stack guard value at the bottom of the new stack */
	move.l	#15, %d0
	move.l	#__boot_stack_bottom, %a0
    15:	move.l	#0xDEADBEEF, (%a0)+
	dbf	%d0, 15b

	jsr	main
	stop	#0x2700			/* halt, since we clobbered the stack on entry */

