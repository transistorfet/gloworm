
#ifndef _STDARG_H
#define _STDARG_H

#ifndef __GNUC_VA_LIST
#define __GNUC_VA_LIST
typedef __builtin_va_list	__gnuc_va_list;
#endif

#if defined __STDC_VERSION__ && __STDC_VERSION__ > 201710L
#define va_start(v, ...)	__builtin_va_start(v, 0)
#else
#define va_start(v, l)		__builtin_va_start(v, l)
#endif

#define va_end(v)		__builtin_va_end(v)
#define va_arg(v, l)		__builtin_va_arg(v, l)
#define va_copy(d, s)		__builtin_va_copy(d, s)

typedef __gnuc_va_list va_list;

#endif

