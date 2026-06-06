/**
 * Common entry point and assertion fail for bare-metal tests
 */

#include <stdio.h>
#include <kconfig.h>

extern int main(void);
extern int init_tty(void);
extern void init_heap(void *addr, unsigned long size);

__attribute__((section(".text.startup")))
void _start(void)
{
	/* Disable interrupts */
	asm volatile("move.w	#0x2700, %sr\n");

	init_tty();
	init_heap((void *) CONFIG_PAGES_START, CONFIG_PAGES_END - CONFIG_PAGES_START);
	main();

	asm volatile("stop	#0x2700\n");
}

void __assert_fail(const char * assertion, const char * file, unsigned int line, const char * function)
{
	printf("%s:%d: %s: Assertion failed `%s`\n", file, line, function, assertion);

	while (1) {}
}

void exit(int exit_code)
{
	printf("exiting test with %d\n", exit_code);

	while (1) {}
}

