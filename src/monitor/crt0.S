
	#include <generated/config.h>

	.global _start
	.global _error
	.global	main
	.global	main

	.section .text

/**
 * Image Start
 */

_start:
	/* Disable interrupts and reset VBR */
	move.w	#0x2700, %sr
	#ifndef CONFIG_MC68000
	clr.l	%d0
	movec	%d0, %vbr
	#endif

	move.w	#0x2000, %sr		/* enable all interrupts */

	bsr.w	main
	stop	#0x2700

