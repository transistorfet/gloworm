
#ifndef _ARCH_TESTAPP_ASM_USERCOPY_H
#define _ARCH_TESTAPP_ASM_USERCOPY_H

#include <stdint.h>
#include <string.h>

#include <kconfig.h>
#include <kernel/utils/usercopy.h>


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
	strncpy(dest, src, max);
	return strlen(dest);
}

static inline int strncpy_to_user(char __user *dest, const char *src, int max)
{
	strncpy(dest, src, max);
	return strlen(src);
}

#endif

