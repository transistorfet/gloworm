
	#include <generated/config.h>

	// Definitions in this file
	.global enter_syscall
	.global enter_irq
	.global create_context
	.global drop_context
	.global set_context_return_value
	.global restore_context
	.global _exit
	.global _sigreturn

	// Declarations defined outside of this file
	.global	syscall_table
	.global kernel_reentries
	.global kernel_stack
	.global current_proc
	.global current_syscall
	.global do_syscall
	.global do_irq
	.global need_reschedule
	.global schedule
	.global bh_requested
	.global run_bh_handlers

	.equ	CONTEXT_SIZE, 60		// The size of the saved registers on the stack (not including the interrupt return)
	.equ	PROC_SP_OFFSET, 8		// The offset into struct process where the stack pointer is located

	.section .text

/**
 * Create a new context given a future stack address, return address
 * 	void *create_context(void *user_stack, void *entry, void *exit);
 */
create_context:
	move.l	(4,%sp), %a0
	move.l	(8,%sp), %a1
	move.l	(12,%sp), %d0

	// Push _exit return address for when main returns
	move.l	%d0, -(%a0)

	// Push the 68010+ Format/Vector Word
	#if !defined(CONFIG_MC68000)
	move.w	#0, -(%a0)
	#endif
	// Push starting address
	move.l	%a1, -(%a0)
	// Push flags
	move.w #0x2000, -(%a0)

	// Push zeros for all registers
	move.l	#0, -(%a0)
	move.l	#0, -(%a0)
	move.l	#0, -(%a0)
	move.l	#0, -(%a0)
	move.l	#0, -(%a0)
	move.l	#0, -(%a0)
	move.l	#0, -(%a0)
	move.l	#0, -(%a0)
	move.l	#0, -(%a0)
	move.l	#0, -(%a0)
	move.l	#0, -(%a0)
	move.l	#0, -(%a0)
	move.l	#0, -(%a0)
	move.l	#0, -(%a0)
	move.l	#0, -(%a0)

	move.l	%a0, %d0
	rts

/**
 * Remove the context from the stack and return the new stack pointer
 * 	void *drop_context(void *user_stack);
 */
drop_context:
	move.l	(4,%sp), %a0
	add.l	#CONTEXT_SIZE, %a0
	#if !defined(CONFIG_MC68000)
	add.l	#2, %a0
	#endif
	add.l	#6, %a0
	move.l	%a0, %d0
	rts

/**
 * Set the return value (%d0) of the context, given the stack pointer
 * 	void set_context_return_value(void *user_stack, uint32_t value);
 */
set_context_return_value:
	move.l	(4, %sp), %a0
	move.l	(8, %sp), (36, %a0)
	rts


.macro SAVE_SYSCALL_REGISTERS
	// TODO change this to use a single movem, but that would be an ABI breaking change
	// Save the arguments in the order expected of the syscall struct
	move.l	%d3, -(%sp)
	move.l	%d2, -(%sp)
	move.l	%a1, -(%sp)
	move.l	%a0, -(%sp)
	move.l	%d1, -(%sp)
	move.l	%d0, -(%sp)
.endm

.macro RESTORE_SYSCALL_REGISTERS
	// Restore the syscall registers
	move.l	(%sp)+, %d0
	move.l	(%sp)+, %d1
	move.l	(%sp)+, %a0
	move.l	(%sp)+, %a1
	move.l	(%sp)+, %d2
	move.l	(%sp)+, %d3
.endm

.macro SAVE_EXTRA_REGISTERS reg=%sp
        // Save registers
	movem.l	%d4-%d7/%a2-%a6, -(\reg)
.endm
 
.macro RESTORE_EXTRA_REGISTERS reg=%sp
        // Restore registers
	movem.l	(\reg)+, %d4-%d7/%a2-%a6
.endm

/**
 * Save all registers to the current user stack and switch to the kernel stack
 */
.macro	SAVE_CONTEXT
	// Increment the number of times we've entered the kernel, and skip the stack switch if we're already inside
	add.l	#1, kernel_reentries
	cmp.l	#1, kernel_reentries
	beq.w	101f

	// We're already on the kernel stack, so just save the scratch registers
	movem.l	%d0-%d1/%a0-%a1, -(%sp)

	bra.w	102f

    101:

	// If we're using user mode, then these will be on the kernel stack (%ssp/%isp) but if
	// we're not, then these will be on the user stack before switching to the kernel stack
	SAVE_SYSCALL_REGISTERS

	// Set the current syscall to the top of the stack
	move.l	%sp, current_syscall

	SAVE_EXTRA_REGISTERS

	#if !defined(CONFIG_M68K_USER_MODE)
	// Switch to the kernel stack
	move.l	current_proc, %a0
	move.l	%sp, (%a0, PROC_SP_OFFSET)
	move.l	kernel_stack, %sp
	#endif

    102:

	// Save the current proc, and compare it against the proc when restoring to know if the full context switch should occur
	move.l	current_proc, previous_proc

/*
	// Increment the number of times we've entered the kernel, and skip the stack switch if we're already inside
	add.l	#1, kernel_reentries
	cmp.l	#1, kernel_reentries
	beq	5f

	move.l	%d7, -(%sp)

	// We're already on the kernel stack, so just save the scratch registers
	move.l	%a1, -(%sp)
	move.l	%a0, -(%sp)
	move.l	%d1, -(%sp)
	move.l	%d0, -(%sp)

	// Save the vector number to %d0 for do_irq
	#if defined(CONFIG_MC68000)
	// TODO not sure how to do this without a custom handler for each which uses the PC value to identify the interrupt
	#else
	move.w	(26,%sp), %d7
	//and.l	#0x00000FFF, %d7
	//lsr.w	#2, %d7
	#endif

	bra	6f

    5:
	#if defined(CONFIG_M68K_USER_MODE)
	// The interrupt switched to supervisor mode, so we're already on the kernel stack

	// Save the original value of the register we'll use as a pointer to the context
	// and copy the user stack pointer into it
	move.l	%a6, -(%sp)
	move.l	%usp, %a6
	move.l	(%sp)+, -(%a6)

	// Save registers
	movem.l	%a5-%a0/%d7-%d0, -(%a6)

	// Save the vector number to %d0 for do_irq
	#if defined(CONFIG_MC68000)
	// TODO not sure how to do this without a custom handler for each which uses the PC value to identify the interrupt
	#else
	move.w	(66,%a6), %d7
	//and.l	#0x00000FFF, %d7
	//lsr.w	#2, %d7
	#endif

	#else
	// Not using user-mode, so we have to manually switch stacks

	// Save registers
	movem.l	%a6-%a0/%d7-%d0, -(%sp)

	// Save the vector number to %d0 for do_irq
	#if defined(CONFIG_MC68000)
	// TODO not sure how to do this without a custom handler for each which uses the PC value to identify the interrupt
	#else
	move.w	(66,%sp), %d7
	//and.l	#0x00000FFF, %d7
	//lsr.w	#2, %d7
	#endif

	// Switch to the kernel stack
	move.l	current_proc, %a5
	adda.l	#PROC_SP_OFFSET, %a5
	move.l	%sp, (%a5)
	move.l	kernel_stack, %sp

	#endif



    6:
	// Save the current proc, and compare it against the proc when restoring to know if the full context switch should occur
	move.l	current_proc, previous_proc
	// Continue processing interrupt
*/

.endm


/**
 * Switch to the current user stack, restore the context, and return from the interrupt
 */
restore_context:
	// If we returning into the kernel, instead of a user process, then skip the next few checks
	cmp.l	#1, kernel_reentries
	bne.w	204f

    202:

	// Check if there are any back half handlers to be run
	cmp.l	#0, bh_requested
	beq.w	203f

	// Enable interrupts while running the back half handlers
	and.w	#0xF8FF, %sr
	bsr	run_bh_handlers
	or.w	#0x0700, %sr

    203:

	// Check if there was a request to schedule a different process
	cmp.l	#0, need_reschedule
	beq.w	204f

	// Enable interrupts while running the scheduler, this might also re-run the syscall that the returning process was blocked on
	and.w	#0xF8FF, %sr
	bsr	schedule
	or.w	#0x0700, %sr

	// After a process has been scheduled and a syscall restarted, it might need to be suspended right away again, so loop over the checks
	bra.w	202b

    204:	// Now we'll actually return, and do a context switch if necessary

	// Decrement the number of times we've entered the kernel, and if this is the last time, then switch to the user stack
	sub.l	#1, kernel_reentries
	beq.w	205f

	// We'll stay on the kernel stack after returning, so just pop off the saved scratch register values and return
	movem.l	(%sp)+, %d0-%d1/%a0-%a1

	rte

    205:	// Returning to a user process, check for context switch

	#if !defined(CONFIG_M68K_USER_MODE)
	// Switch to the user stack
	move.l  %sp, kernel_stack
	move.l  current_proc, %a0
	move.l  (%a0, PROC_SP_OFFSET), %sp
	#endif

/*
	// Skip the extended context switch if the proc hasn't changed
	move.l  (previous_proc), %a0
	cmpa.l  (current_proc), %a0
	beq.w   206f

	#if defined(CONFIG_M68K_USER_MODE)
	// The syscall regs are still on the kernel stack, which is in %sp
	RESTORE_SYSCALL_REGISTERS
	// TODO don't you actually need USP since it wasn't saved in the other switch?
	move.l  previous_proc, %sp
	move.l  (%sp, PROC_SP_OFFSET), %sp
	// Reserve 2 long words for the exception frame
	sub.l   #8, %sp
	SAVE_SYSCALL_REGISTERS

	move.l  kernel_stack, %sp
	move.l  %a0, (%sp, 6)
	move.l  %a1, (%sp, 8)

	// Load the previous process's user stack pointer into a0
	move.l  (%a0, PROC_SP_OFFSET), %a0
	// TODO complete this
	move.l (%a0)+, -(%sp)
	#endif

	// Switch to the previous process's stack
	move.l  previous_proc, %a0
	move.l  (%a0,PROC_SP_OFFSET), %a1

	// Save the extended context of the previously running process, not including the system call registers
	SAVE_EXTRA_REGISTERS %a1

	// Update the new stack pointer of the previous proc
	move.l  %a1, (%a0,PROC_SP_OFFSET)

	// Restore the extended context of the next process, not including the system call registers
	RESTORE_EXTRA_REGISTERS

    206:	// Now it's the last step before returning to a user process

*/
	// Restore registers
	RESTORE_EXTRA_REGISTERS
	RESTORE_SYSCALL_REGISTERS

	/*
	#if defined(CONFIG_M68K_USER_MODE)

	move.l  current_proc, %sp
	adda.l  #PROC_SP_OFFSET, %sp

	// Restore registers
	RESTORE_SYSCALL_REGISTERS
	RESTORE_EXTRA_REGISTERS

	move.l	%sp, %usp

	#else

	// Switch back to the current process's stack
	move.l	%sp, kernel_stack
	move.l	current_proc, %a0
	move.l	(%a0, PROC_SP_OFFSET), %sp

	// Restore registers
	RESTORE_SYSCALL_REGISTERS
	RESTORE_EXTRA_REGISTERS

	#endif
	*/

	rte


/**
 * System Call Entry Point
 *
 * This is the handler for the system call trap.
 * The system call number (index) is passed in d0, and arguments are passed
 * in order as d1, a0, and a1 (only 3 arguments max atm)
 */
enter_syscall:
	// Disable ints and save the context
	or.w	#0x0700, %sr
	SAVE_CONTEXT

	// Save the arguments in the order expected of the syscall struct
	//SAVE_SYSCALL_REGISTERS

	// Enable interrupts during the syscall
	move.l	%d7, -(%sp)
	and.w	#0xF8FF, %sr
	jsr	do_syscall
	or.w	#0x0700, %sr
	add.l	#4, %sp

	// Restore the syscall registers
	//RESTORE_SYSCALL_REGISTERS

	// Jump to the syscall interrupt return
	bra	restore_context


/**
 * Serial Controller IRQ
 *
 * This will save the context and stack frame before calling the C function to handle the rest
 */
enter_irq:
	SAVE_CONTEXT

	move.l	%d7, -(%sp)
	bsr	do_irq
	add.l	#4, %sp

	// Jump to the syscall interrupt return
	bra	restore_context


_exit:
	// Execute the exit syscall to terminate the current process
	moveq	#1, %d0
	trap	#1

	// This shouldn't run because the syscall should never return
	stop	#0x2000


_sigreturn:
	// Execute the sigreturn syscall to clean up the signal handling
	moveq	#30, %d0
	trap	#1

	// This shouldn't run because the syscall should never return
	stop	#0x2000


//////////////////////
// Global Variables
//////////////////////
	.section .data
 
// Stores the previous process on entering from a user process
// Used to compare if returning to the same process, in which case an extended
// context switch is performed
previous_proc:
	.long	0

