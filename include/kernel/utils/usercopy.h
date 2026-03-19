
#ifndef _MM_USERCOPY_H
#define _MM_USERCOPY_H

#include <stdint.h>
#include <kconfig.h>

/// A marker that signifies the given address is relative to the user address space
#define __user

static inline uint8_t get_user_uint8(const void __user *src);
static inline void put_user_uint8(void __user *dest, uint8_t value);
static inline uintptr_t get_user_uintptr(const void __user *src);
static inline void put_user_uintptr(void __user *dest, uintptr_t value);

static inline int memcpy_from_user(void *dest, const void __user *src, int n);
static inline int memcpy_to_user(void __user *dest, const void *src, int n);
static inline int strncpy_from_user(char *dest, const char __user *src, int max);
static inline int strncpy_to_user(char __user *dest, const char *src, int max);


///// LOAD ARCH-SPECIFIC USERCOPY
#include <asm/usercopy.h>
///// END ARCH-SPECIFIC USERCOPY


#if defined(CONFIG_MMU)

#define COPY_USER_STRING(var_name, max)						\
	char var_name##_buffer[max] = { 0 };					\
	strncpy_from_user((var_name##_buffer), var_name, max);			\
	var_name = var_name##_buffer;

#define COPY_USER_STRUCT(type, var_name)					\
	typeof(type) var_name##_buffer;						\
	memcpy_from_user((&var_name##_buffer), var_name, sizeof(type));		\
	var_name = &var_name##_buffer;

#else	// CONFIG_MMU

#define COPY_USER_STRING(var_name, max)						{}

#define COPY_USER_STRUCT(type, var_name)					{}

#endif	// CONFIG_MMU

#endif

