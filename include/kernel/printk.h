
#ifndef _KERNEL_PRINTK_H
#define _KERNEL_PRINTK_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <kconfig.h>

#if defined(CONFIG_LOG_LEVEL_TRACE)
#define LOG_LEVEL	LOG_TRACE
#elif defined(CONFIG_LOG_LEVEL_DEBUG)
#define LOG_LEVEL	LOG_DEBUG
#elif defined(CONFIG_LOG_LEVEL_INFO)
#define LOG_LEVEL	LOG_INFO
#else
#define LOG_LEVEL	LOG_NOTICE
#endif

#define LOG_FATAL		0
#define LOG_CRITICAL		1
#define LOG_ERROR		2
#define LOG_WARNING		3
#define LOG_NOTICE		4
#define LOG_INFO		5
#define LOG_DEBUG		6
#define LOG_TRACE		7

int vprintk(int buffered, const char *fmt, va_list args);
int printk_direct(const char *fmt, ...);
int printk_buffered(const char *fmt, ...);

__attribute__((noreturn)) void panic(const char *fmt, ...);

void printk_dump(void *ptr, size_t length);
void printk_dump_bytes(void *ptr, size_t length);

// TODO this has been set to direct because otherwise the buffer fills up, and everything slows to a crawl
#define printk(...)		printk_direct(__VA_ARGS__)

#define log_fatal(...)		printk(__VA_ARGS__)
#define log_critical(...)	printk(__VA_ARGS__)
#define log_error(...)		printk(__VA_ARGS__)
#define log_warning(...)	printk(__VA_ARGS__)

#if LOG_LEVEL >= LOG_NOTICE
#define log_notice(...)		printk(__VA_ARGS__)
#else
#define log_notice(...)
#endif

#if LOG_LEVEL >= LOG_INFO
#define log_info(...)		printk(__VA_ARGS__)
#else
#define log_info(...)
#endif

#if LOG_LEVEL >= LOG_DEBUG
#define log_debug(...)		printk(__VA_ARGS__)
#else
#define log_debug(...)
#endif

#if LOG_LEVEL >= LOG_TRACE
#define log_trace(...)		printk(__VA_ARGS__)
#else
#define log_trace(...)
#endif

// Debug LEDS
#if defined(CONFIG_DEBUG_LEDS)
#define DBGLED0		0x01
#define DBGLED1		0x02
#define DBGLED2		0x04
#define DBGLED3		0x08
void debug_leds_set(uint8_t bits);
void debug_leds_reset(uint8_t bits);
void debug_leds_toggle(uint8_t bits);
#endif

// MMU-specific functions
#if defined(CONFIG_MMU)
#include <asm/addresses.h>

struct memory_map;

void printk_dump_user(struct memory_map *map, virtual_address_t vaddr, size_t length);
#endif

#endif

