/**
 * Common entry point and assertion fail for bare-metal tests
 */

#include <kernel/printk.h>

extern int main(void);
extern int init_tty(void);

void _start(void)
{
	/* Disable interrupts */
	asm volatile("move.w	#0x2700, %sr\n");

	init_tty();
	main();

	asm volatile("stop	#0x2700\n");
}

void __assert_fail(const char * assertion, const char * file, unsigned int line, const char * function)
{
	printk("%s:%d: %s: Assertion failed `%s`\n", file, line, function, assertion);

	while (1) {}
}

void exit(int exit_code)
{
	printk("exiting test with %d\n", exit_code);

	while (1) {}
}

