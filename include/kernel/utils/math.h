
#ifndef _KERNEL_UTILS_MATH_H
#define _KERNEL_UTILS_MATH_H

#include <sys/param.h>

#define rounddown(value, size)	((value) & ~((size) - 1))

#define alignment_offset(value, size)	((value) & ((size) - 1))

#endif

