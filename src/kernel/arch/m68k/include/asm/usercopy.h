
#ifndef _ARCH_M68K_ASM_USERCOPY_H
#define _ARCH_M68K_ASM_USERCOPY_H

#include <stdint.h>
#include <string.h>

#include <kconfig.h>
#include <kernel/proc/process.h>
#include <kernel/proc/memory.h>
#include <kernel/utils/usercopy.h>

#define M68K_FC_USER_DATA		1
#define M68K_FC_USER_PROGRAM		2
#define M68K_FC_SUPERVISOR_DATA		5
#define M68K_FC_SUPERVISOR_PROGRAM	6
#define M68K_FC_CPU_SPACE		7

#if defined(CONFIG_MMU)


static inline uint8_t get_user_uint8(const void __user *src)
{
	uint8_t result = 0;

	// TODO this is really a problem for performance to have this instruction before every byte copy
	// can we make a "start iter operation" function which sets this up, and then the subsequent calls are single byte only?
	asm volatile("movec.l	%0, %%sfc\n" : : "d" (M68K_FC_USER_DATA) :);

	//asm volatile(
	//	"moves.b	(%1), %%d0\n"
	//	"move.b		%%d0, (%0)\n"
	//	: : "a" (&result), "a" (src) : "d0"
	//);
	asm volatile(
		"moves.b	(%1), %0\n"
		: "=d" (result) : "a" (src) :
	);
	return result;
}

static inline void put_user_uint8(void __user *dest, uint8_t value)
{
	// TODO this is really a problem for performance to have this instruction before every byte copy
	// can we make a "start iter operation" function which sets this up, and then the subsequent calls are single byte only?
	asm volatile("movec.l	%0, %%dfc\n" : : "d" (M68K_FC_USER_DATA) :);

	asm volatile(
		"moves.b	%0, (%1)\n"
		: : "d" (value), "a" (dest) :
	);
}


static inline uintptr_t get_user_uintptr(const void __user *src)
{
	uintptr_t result;

	asm volatile("movec.l	%0, %%sfc\n" : : "d" (M68K_FC_USER_DATA) :);

	asm volatile(
		"moves.l	(%1), %0\n"
		: "=d" (result) : "a" (src) :
	);
	return result;
}

static inline void put_user_uintptr(void __user *dest, uintptr_t value)
{
	asm volatile("movec.l	%0, %%dfc\n" : : "d" (M68K_FC_USER_DATA) :);

	asm volatile(
		"moves.l	%0, (%1)\n"
		: : "d" (value), "a" (dest) :
	);
}
static inline int memcpy_from_user(void *dest, const void __user *src, int n)
{
	int bytes;

	asm volatile("movec.l	%0, %%sfc\n" : : "d" (M68K_FC_USER_DATA) :);

	// TODO optimize this by transferring long words instead of single bytes
	for (bytes = 0; bytes < n; bytes++, dest++, src++) {
		asm volatile(
			"moves.b	(%1), %%d0\n"
			"move.b		%%d0, (%0)\n"
			: : "a" (dest), "a" (src) : "d0"
		);
	}
	return bytes;
}

static inline int memcpy_to_user(void __user *dest, const void *src, int n)
{
	int bytes;

	asm volatile("movec.l	%0, %%dfc\n" : : "d" (M68K_FC_USER_DATA) :);

	// TODO optimize this by transferring long words instead of single bytes
	for (bytes = 0; bytes < n; bytes++, dest++, src++) {
		asm volatile(
			"move.b		(%1), %%d0\n"
			"moves.b	%%d0, (%0)\n"
			: : "a" (dest), "a" (src) : "d0"
		);
	}
	return bytes;
}

static inline int strncpy_from_user(char *dest, const char __user *src, int max)
{
	int bytes;
	char temp = 1;

	asm volatile("movec.l	%0, %%sfc\n" : : "d" (M68K_FC_USER_DATA) :);

	// TODO optimize this by transferring long words instead of single bytes
	for (bytes = 0; temp != 0 && bytes < max; bytes++, dest++, src++) {
		asm volatile(
			"moves.b	(%2), %0\n"
			"move.b		%0, (%1)\n"
			: "=d" (temp) : "a" (dest), "a" (src) :
		);
	}
	return bytes;
}

static inline int strncpy_to_user(char __user *dest, const char *src, int max)
{
	int bytes;
	char temp = 1;

	asm volatile("movec.l	%0, %%dfc\n" : : "d" (M68K_FC_USER_DATA) :);

	// TODO optimize this by transferring long words instead of single bytes
	for (bytes = 0; temp != 0 && bytes < max; bytes++, dest++, src++) {
		asm volatile(
			"move.b		(%2), %0\n"
			"moves.b	%0, (%1)\n"
			: "=d" (temp) : "a" (dest), "a" (src) :
		);
	}
	return bytes;
}

#else // CONFIG_MMU

static inline uint8_t get_user_uint8(const void __user *src)
{
	return *((uint8_t *) src);
}

static inline void put_user_uint8(void __user *dest, uint8_t value)
{
	*((uint8_t *) dest) = value;
}

static inline uintptr_t get_user_uintptr(const void __user *src)
{
	return *((uintptr_t *) src);
}

static inline void put_user_uintptr(void __user *dest, uintptr_t value)
{
	*((uintptr_t *) dest) = value;
}

static inline int memcpy_from_user(void *dest, const void __user *src, int n)
{
	memcpy(dest, src, n);
	return n;
}

static inline int memcpy_to_user(void __user *dest, const void *src, int n)
{
	memcpy(dest, src, n);
	return n;
}

static inline int strncpy_from_user(char *dest, const char __user *src, int max)
{
	int size;

	strncpy(dest, src, max);
	size = strnlen(dest, max);
	if (size == max) {
		dest[max - 1] = '\0';
	}
	return size;
}

static inline int strncpy_to_user(char __user *dest, const char *src, int max)
{
	int size;

	strncpy(dest, src, max);
	size = strnlen(dest, max);
	if (size == max) {
		dest[max - 1] = '\0';
	}
	return size;
}

#endif // CONFIG_MMU

#endif

