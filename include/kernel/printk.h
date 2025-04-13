
#ifndef _SRC_KERNEL_PRINTK_H
#define _SRC_KERNEL_PRINTK_H

#include <stdarg.h>
#include <stdint.h>

int vprintk(int buffered, const char *fmt, va_list args);
int printk(const char *fmt, ...);
int printk_safe(const char *fmt, ...);

void printk_data(uint16_t *data, uint32_t length);

void prepare_for_panic();
__attribute__((noreturn)) void panic(const char *fmt, ...);

#endif

