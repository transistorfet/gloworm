
	.global _sigreturn

	.section .text

_sigreturn:
	// Execute the sigreturn syscall to clean up the signal handling
	moveq	#30, %d0
	trap	#1

	// This shouldn't run because the syscall should never return
	stop	#0x2000

