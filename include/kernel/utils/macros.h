
#ifndef _KERNEL_UTILS_MATH_H
#define _KERNEL_UTILS_MATH_H

#include <sys/param.h>

#define rounddown(value, size)	((value) & ~((size) - 1))

#define alignment_offset(value, size)	((value) & ((size) - 1))

#define containerof(ttype, item, member)	\
	((ttype *) (((char *) (item)) - offsetof((ttype), (member))))

#define roundup_power_of_2(value)		\
	(1 << (32 - __builtin_clz((value) - 1)))

#endif

