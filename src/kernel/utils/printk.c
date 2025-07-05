
#include <stdio.h>
#include <stdint.h>
#include <asm/irqs.h>
#include <kernel/printk.h>


#define PRINTK_BUFFER	128

/// Functions that must be provided by the console driver
void console_prepare_for_panic();
int console_putchar_direct(int ch);
int console_putchar_buffered(int ch);


int vprintk(int buffered, const char *fmt, va_list args)
{
	int i;
	char buffer[PRINTK_BUFFER];
	int (*put)(int) = buffered ? console_putchar_buffered : console_putchar_direct;

	vsnprintf(buffer, PRINTK_BUFFER, fmt, args);
	for (i = 0; i < PRINTK_BUFFER && buffer[i]; i++) {
		put(buffer[i]);
	}
	return i;
}

int printk_direct(const char *fmt, ...)
{
	int ret;
	va_list args;

	va_start(args, fmt);
	ret = vprintk(1, fmt, args);
	va_end(args);

	return ret;
}

int printk(const char *fmt, ...)
{
	int ret;
	va_list args;

	va_start(args, fmt);
	ret = vprintk(0, fmt, args);
	va_end(args);

	return ret;
}

__attribute__((noreturn)) void panic(const char *fmt, ...)
{
	va_list args;

	console_prepare_for_panic();

	va_start(args, fmt);
	vprintk(0, fmt, args);
	va_end(args);

	extern void print_stack(void *);
	print_stack((void *) args);

	HALT();
	__builtin_unreachable();
}

void printk_dump(uint16_t *data, uint32_t length)
{
	length >>= 1; // Adjust the dump size from bytes to words
	while (length > 0) {
		printk("%x: ", (unsigned int) data);
		for (short i = 0; i < 8 && length > 0; i++, length--) {
			printk("%04x ", data[i]);
		}
		printk("\n");
		data += 8;
	}
	printk("\n");
}

