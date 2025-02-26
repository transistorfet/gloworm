
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
	/* Disable interrupts */
	move.w	#0x2700, %sr
	/* Reset the VBR if on 68010+ */
	#if !defined(CONFIG_MC68000)
	clr.l	%d0
	movec	%d0, %vbr
	#endif

	move.w	#0x2000, %sr		/* enable all interrupts */

	bsr.w	main
	stop	#0x2700

