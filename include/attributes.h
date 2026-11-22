
#ifndef _ATTRIBUTES_H
#define _ATTRIBUTES_H

#if defined(__GNUC__)
#define __unused		__attribute__((unused))
#define __FALLTHROUGH__		__attribute__((fallthrough))
#else
#error "non-gnu compiler detected, but no attributes defined for it"
#endif

#endif

