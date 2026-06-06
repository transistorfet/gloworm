
#ifndef _KERNEL_UTILS_MACROS_H
#define _KERNEL_UTILS_MACROS_H

#include <sys/param.h>

#define align_up(value, size)		(((value) + ((size) - 1)) & ~((size) - 1))
#define align_down(value, size)		((value) & ~((size) - 1))
#define align_remainder(value, size)	((value) & ((size) - 1))

#define containerof(ttype, item, member)	\
	((ttype *) (((char *) (item)) - offsetof((ttype), (member))))

#define roundup_next_pow2(value)		\
	(1 << (32 - __builtin_clz((value) - 1)))

#endif

