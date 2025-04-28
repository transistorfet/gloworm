
	#include <kconfig.h>

	.global _start
	.global	main
	.global boot_args
	.global __bss_start
	.global __bss_end

/**
 * Image Start
 */

.section    .text.start
_start:
	/* Disable interrupts */
	move.w  #0x2700, %sr

	/* Zero out the BSS section of the kernel */
	lea.l	__bss_start, %a0
	move.l	__bss_end, %d0
	sub.l	%a0, %d0
	lsr.l	#2, %d0
    5:
	clr.l	(%a0)+
	dbne	%d0, 5b

	/* Save boot args from the old stack before re-initializing */
	movea.l	(+4,%sp), %a0
	lea	boot_args, %a1
    S1: move.b	(%a0)+, (%a1)+
	bne	S1

	move.l	#0x120000, %sp		/* re-initialize the stack pointer to the top of the kernel space (lower 64KB of RAM) */
	jsr	main
	stop	#0x2700			/* halt, since we clobbered the stack on entry */

