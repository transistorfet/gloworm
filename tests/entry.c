/**
 * Common entry point and assertion fail for bare-metal tests
 */

#include <stdio.h>

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
	printf("%s:%d: %s: %s\n", file, line, function, assertion);

	while (1) {}
}

