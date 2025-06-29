
	#include <generated/config.h>

	// Definitions in this file
	.global enter_syscall
	.global enter_irq
	.global enter_exception
	.global create_context
	.global drop_context
	.global restore_context
	.global _exit
	.global _sigreturn
	.global __do_fork
	.global __do_clone
	.global __do_sigreturn

	// Declarations defined outside of this file
	.global	syscall_table
	.global kernel_reentries
	.global kernel_stack
	.global current_proc
	.global previous_proc
	.global current_syscall
	.global do_syscall
	.global do_irq
	.global need_reschedule
	.global schedule
	.global bh_requested
	.global run_bh_handlers
	.global mmu_table_create

	.equ	SHORT_CONTEXT_SIZE, 24		// The size of the syscall registers on the stack (not including the interrupt return)
	.equ	FULL_CONTEXT_SIZE, 64		// The size of the saved registers on the stack (not including the interrupt return)
	.equ	PROC_SP_OFFSET, 8		// The offset into struct process where the stack pointer is located
	.equ	PROC_RETURN_OFFSET, 12		// The offset into struct process where the return_value is located

	.section .text

/**
 * Create a new context given a future stack address, return address
 * 	void *create_context(void *kernel_stack, void *entry, void *exit, void *user_stack);
 */
create_context:
	movea.l	(4,%sp), %a0	// void *kernel_stack
	movea.l	(8,%sp), %a1	// void *entry
	move.l	(12,%sp), %d0	// void *exit
	move.l	(16,%sp), %d1	// void *user_stack

/*
	// Push _exit return address for when main returns
	move.l	%d0, -(%a0)
*/

	#if !defined(CONFIG_MC68000)
	// Push the 68010+ Format/Vector Word
	move.w	#0, -(%a0)
	#endif

	// Push starting address
	move.l	%a1, -(%a0)

	// Push flags
	#if defined(CONFIG_M68K_USER_MODE)
	move.w #0x0000, -(%a0)
	#else
	move.w #0x2000, -(%a0)
	#endif

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
	move.l	%d1, -(%a0)
	// Save context size as the last item
	move.l  #FULL_CONTEXT_SIZE, -(%a0)

	move.l	%a0, %d0
	rts

/**
 * Remove the context from the stack and return the new stack pointer
 * 	void *drop_context(void *kernel_stack);
 */
drop_context:
	movea.l	(4,%sp), %a0
	move.l  (%a0)+, %d0
	add.l   %d0, %a0
	#if !defined(CONFIG_MC68000)
	add.l	#2, %a0
	#endif
	add.l	#6, %a0
	move.l	%a0, %d0
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
	movea.l	(%sp)+, %a0
	movea.l	(%sp)+, %a1
	move.l	(%sp)+, %d2
	move.l	(%sp)+, %d3
.endm

.macro SAVE_EXTRA_REGISTERS reg=%sp
	// Save registers
	movem.l	%d4-%d7/%a2-%a6, -(\reg)

	// TODO should you just always load/restore it, which normally would have no effect and be a bit slower (two instructions)
	//	but it would preserve the value, which could be used by something else?, but really... would it ever?
	#if defined(CONFIG_M68K_USER_MODE)
	movec.l	%usp, %d4
	move.l	%d4, -(\reg)
	#else
	move.l	#0, -(\reg)
	#endif
.endm

.macro RESTORE_EXTRA_REGISTERS reg=%sp
	#if defined(CONFIG_M68K_USER_MODE)
	move.l	(\reg)+, %d4
	movec.l	%d4, %usp
	#else
	add.l	#4, \reg
	#endif

	// Restore registers
	movem.l	(\reg)+, %d4-%d7/%a2-%a6

.endm

/// Save all registers to the current user stack and switch to the kernel stack
///
/// After this macro, the %a1 register will have a pointer to the exception frame on the thread's stack
.macro	SAVE_CONTEXT
	// Increment the number of times we've entered the kernel, and skip the stack switch if we're already inside
	add.l	#1, kernel_reentries
	cmp.l	#1, kernel_reentries
	beq.w	101f

	// We're already on the kernel stack, so just save the scratch registers
	movem.l	%d0-%d1/%a0-%a1, -(%sp)

	// Load a pointer to the exception frame into %a1
	lea.l	(%sp, 16), %a1

	bra.w	102f

    101:	// Entering from user-mode

	// If we're using user mode, then these will be on the kernel stack (%ssp/%isp) but if
	// we're not, then these will be on the user stack before switching to the kernel stack
	SAVE_SYSCALL_REGISTERS

	// Set the current syscall to the top of the stack
	move.l	%sp, current_syscall

	// Load a pointer to the exception frame into %a1
	lea.l	(%sp, SHORT_CONTEXT_SIZE), %a1

	// Save the context size now that the syscall is saved
	move.l  #SHORT_CONTEXT_SIZE, -(%sp)

	movea.l	current_proc, %a0

	// Save the original version of d0 to be returned on exit if not a syscall
	move.l	%d0, (%a0, PROC_RETURN_OFFSET)

	// Switch to the internal kernel stack
	move.l	%sp, (%a0, PROC_SP_OFFSET)
	movea.l	kernel_stack, %sp

	// Save the current proc, and compare it against the proc when restoring to know if the full context switch should occur
	move.l	%a0, previous_proc

    102:	// Everything is saved in both user and kernel cases, so now do anything common to both before continuing

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

	movea.l  current_proc, %a0
	movea.l  previous_proc, %a1

	///// A0 = CURRENT_PROC, A1 = PREVIOUS_PROC /////

	// Save the kernel stack before the switch
	move.l  %sp, kernel_stack

	// Skip if no previous process
	cmpa.l  #0, %a1
	beq.w   207f
	// Skip the extended context switch if the proc hasn't changed
	cmpa.l  %a0, %a1
	beq.w   207f

	// Switch to the previous process's stack
	movea.l  (%a1, PROC_SP_OFFSET), %sp

	// Save the extended context of the previously running process, not including the system call registers
	move.l  (%sp)+, %d1
	cmp.l   #FULL_CONTEXT_SIZE, %d1
	beq.l   206f
	SAVE_EXTRA_REGISTERS
    206:
	move.l  #FULL_CONTEXT_SIZE, -(%sp)     // push a new context size

	// Update the new stack pointer of the previous proc
	move.l  %sp, (%a1, PROC_SP_OFFSET)

    207:

	// Switch to the current process's user stack
	movea.l  (%a0, PROC_SP_OFFSET), %sp

	// Check the context size
	move.l  (%sp)+, %d1
	cmp.l   #FULL_CONTEXT_SIZE, %d1
	bne.s   208f

	// Restore the extended context of the next process, not including the system call registers
	RESTORE_EXTRA_REGISTERS

    208:       // Now it's the last step before returning to a user process

	// This is the last time %a0 will be available as the current proc
	// Stick the return value directly onto the stack where d0 will be
	// popped from, so it won't get overwritten by the next macro
	move.l	(%a0, PROC_RETURN_OFFSET), (%sp)

	RESTORE_SYSCALL_REGISTERS

	rte


/// Generic entry point for some internal syscall
///
/// Some syscalls need the full set of registers saved to the stack, such as
/// fork/clone so that the cloned child can restore the correct extra register
/// values, which can contain important information, even if the child process
/// is just calling exec() soon after.  The system is unbootable without this.
.macro SAVE_FULL_SYSCALL_ENTRY entry_name, handler_name
\entry_name:
	// Save the extra registers to the user stack before cloning the process
	// It's ok to clobber the scratch registers because they're already saved
	move.l  current_proc, %a0
	move.l  (%a0, PROC_SP_OFFSET), %a1
	add.l   #4, %a1				// remove the old context size from the stack
	SAVE_EXTRA_REGISTERS %a1
	move.l  #FULL_CONTEXT_SIZE, -(%a1)	// replace it with the new context size
	move.l  %a1, (%a0, PROC_SP_OFFSET)

	// Do the actual system call
	jsr     \handler_name

	// Remove the registers from the stack since `restore_context` will assume they aren't there
	// The original values should still be in the registers, so there's no need to load them
	move.l  current_proc, %a0
	//sub.l #36, (%a0, PROC_SP_OFFSET)
	move.l  (%a0, PROC_SP_OFFSET), %a1
	add.l   #4, %a1				// remove the old context size from the stack
	RESTORE_EXTRA_REGISTERS %a1
	move.l  #SHORT_CONTEXT_SIZE, -(%a1)	// replace it with the new context size
	move.l  %a1, (%a0, PROC_SP_OFFSET)

	rts
.endm

SAVE_FULL_SYSCALL_ENTRY __do_fork, do_fork
SAVE_FULL_SYSCALL_ENTRY __do_clone, do_clone
SAVE_FULL_SYSCALL_ENTRY __do_sigreturn, do_sigreturn


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

	// Default to returning -1 on exit.  The syscall return will likely overwrite this before exiting
	move.l	#-1, (%a0, PROC_RETURN_OFFSET)

	// Save the irq number on the stack
	//move.l	%d0, -(%sp)

	// Enable interrupts during the syscall
	and.w	#0xF8FF, %sr

	jsr	do_syscall

	// Disable interrupts after the syscall
	or.w	#0x0700, %sr

	// Pop the irq number off the stack
	//adda.l	#4, %sp

	// Jump to the syscall interrupt return
	bra	restore_context


/**
 * IRQ Entry Point
 *
 * This will save the context and stack frame before calling the C function to handle the irq
 */
enter_irq:
	SAVE_CONTEXT

	#if defined(CONFIG_MC68000)
	// TODO not sure how to do this without a custom handler for each which uses the PC value to identify the interrupt
	#else
	// Fetch the vector number from the exception frame
	move.w	(%a1, 6), %d0
	lsr.w	#2, %d0
	and.l	#0x00000FFF, %d0
	#endif

	// Save the irq number on the stack
	move.l	%d0, -(%sp)

	bsr	do_irq

	// Pop the irq number off the stack
	adda.l	#4, %sp

	// Jump to the syscall interrupt return
	bra	restore_context


/**
 * CPU Exception Entry Point
 *
 * This will always save the full context and then call the exception handler to decide what to do from there
 */
enter_exception:
	// Disable irqs
	or.w    #0x0700, %sr

	// TODO this would be wrong for user mode
	SAVE_SYSCALL_REGISTERS
	SAVE_EXTRA_REGISTERS

	movea.l  %sp, %a5

	// Switch to the internal kernel stack
	movea.l  current_proc, %a0
	move.l  %sp, (%a0, PROC_SP_OFFSET)
	movea.l  kernel_stack, %sp

	bra.w   handle_exception


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

