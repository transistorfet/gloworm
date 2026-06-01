
#ifndef _ASSERT_H
#define _ASSERT_H

#ifdef	NDEBUG

#define assert(expr)		((void) (0))

#else	// NDEBUG

extern void __assert_fail(const char *__assertion, const char *__file, unsigned int __line, const char *__function) __attribute__ ((__noreturn__));

#define assert(expr)		(((char) (expr)) ? (void) (0) : __assert_fail(#expr, __FILE__, __LINE__, __func__))

#endif	// NDEBUG

#endif	// _ASSERT_H

