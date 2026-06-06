
#ifndef _SYS_PARAM_H
#define _SYS_PARAM_H

#define howmany(x, y)		(((x) + ((y) - 1)) / (y))
#define roundup(x, step)	(((x) + (step) - 1) & ~((step) - 1))
#define powerof2(x)		((((x) - 1) & (x)) == 0)

#define MIN(a, b)		(((a) < (b)) ? (a) : (b))
#define MAX(a, b)		(((a) > (b)) ? (a) : (b))

#endif

