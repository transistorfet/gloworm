
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <kernel/printk.h>


#define PRINTK_BUFFER	512


int printk_direct(const char *fmt, ...)
{
	int ret;
	va_list args;

	va_start(args, fmt);
	ret = vprintf(fmt, args);
	va_end(args);

	return ret;
}

int printk(const char *fmt, ...)
{
	int ret;
	va_list args;

	va_start(args, fmt);
	ret = vprintf(fmt, args);
	va_end(args);

	return ret;
}

__attribute__((noreturn)) void panic(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);

	//extern void print_stack(void *);
	//print_stack((void *) args);

	printk("exiting...\n");
	exit(-1);
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

void printk_dump_bytes(uint8_t *data, uint32_t length)
{
	while (length > 0) {
		printk("%x: ", (uintptr_t) data);
		for (short i = 0; i < 16 && length > 0; i++, length--) {
			printk("%02x ", data[i]);
		}
		printk("\n");
		data += 16;
	}
	printk("\n");
}

