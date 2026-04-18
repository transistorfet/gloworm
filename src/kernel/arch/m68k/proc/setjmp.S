
	.global	setjmp
	.global	longjmp

	.section	.text

setjmp:
	move.l	(4,%sp), %a0	// get the jmp_buf arg
	move.l	(%sp)+, %a1	// pop the return address so the stack looks like how it will after `rts`
	move.l	%sp, (4,%a0)	// save the stack into buffer
	move.l  %a1, (%a0)	// save the return address into buffer
	move.l  %a1, -(%sp)	// put the return address back on the stack
	clr.l	%d0
	rts

longjmp:
	move.l	(4,%sp), %a0	// get the jmp_buf
	move.l	(8,%sp), %d0	// get the return value
	move.l	(4,%a0), %sp	// restore the stack pointer
	move.l	(%a0), %a1	// get the return address
	jmp.l	(%a1)

