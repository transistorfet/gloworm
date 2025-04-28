
#ifndef _KERNEL_SYSCALL_H
#define _KERNEL_SYSCALL_H

#include <errno.h>
#include <asm/syscall.h>

typedef int (*syscall_t)(int, int, int, int, int);

struct syscall_record {
	int syscall;
	int arg1;
	int arg2;
	int arg3;
	int arg4;
	int arg5;
};

static inline void SYSCALL_SAVE(struct syscall_record *r, int n, int a1, int a2, int a3, int a4, int a5)
{
	r->syscall = n;
	r->arg1 = a1;
	r->arg2 = a2;
	r->arg3 = a3;
	r->arg4 = a4;
	r->arg5 = a5;
}

#endif
