
#include <stdio.h>
#include <stdint.h>
#include <asm/irqs.h>
#include <kernel/printk.h>

#define DIRECT		0
#define BUFFERED	1


#define PRINTK_BUFFER	512

/// Functions that must be provided by the console driver
void console_prepare_for_panic(void);
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
	ret = vprintk(DIRECT, fmt, args);
	va_end(args);

	return ret;
}

int printk_buffered(const char *fmt, ...)
{
	int ret;
	va_list args;

	va_start(args, fmt);
	ret = vprintk(BUFFERED, fmt, args);
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

void printk_dump(void *ptr, size_t length)
{
	uint16_t *data = ptr;

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

void printk_dump_bytes(void *ptr, size_t length)
{
	uint8_t *data = ptr;

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

#if defined(CONFIG_MMU)
#include <kernel/utils/iovec.h>
#include <asm/mmu.h>
#include <kernel/mm/map.h>

void printk_dump_user(struct memory_map *map, virtual_address_t vaddr, size_t length)
{
	short i;
	short index = 0;
	struct kvec kvec[3];
	short seg = 0, max_segs;

	max_segs = memory_map_load_pages_into_kvec(map, kvec, 3, vaddr, length, 0);
	if (max_segs < 0) {
		printk("error fetching page for %x: %d\n", vaddr, max_segs);
		return;
	}

	while (length > 0) {
		printk("%08x: ", (unsigned int) &kvec[seg].buf[index]);
		for (i = 0; i < 8 && length > 0; i++, index += 2, length -= 2) {
			if (index >= kvec[seg].bytes) {
				index = 0;
				seg += 1;
				if (seg >= max_segs) {
					printk("\nlimit reached\n");
					return;
				}
			}
			printk("%04x ", *((uint16_t *) &kvec[seg].buf[index]));
		}
		printk("\n");
	}
	printk("\n");
}
#endif

