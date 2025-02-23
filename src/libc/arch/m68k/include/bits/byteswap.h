 
#ifndef _M68K_BITS_BYTESWAP_H
#define _M68K_BITS_BYTESWAP_H

#include <stdint.h>

#define BIG_ENDIAN	2
#define LITTLE_ENDIAN	3

#define BYTE_ORDER	BIG_ENDIAN

static inline uint16_t __bswap_16(uint16_t x)
{
	asm volatile("rol.w	#8, %0\n" : "+md" (x));
	return x;
}

static inline uint32_t __bswap_32(uint32_t x)
{
	asm volatile(
	"rol.w	#8, %0\n"
	"swap	%0\n"
	"rol.w	#8, %0\n"
	: "+md" (x)
	);
	return x;
}

static inline uint64_t __bswap_64(uint64_t x)
{
	return ((uint64_t) __bswap_32((uint32_t) (x >> 32))) | (((uint64_t) __bswap_32((uint32_t) (x & 0xFFFFFFFF))) << 32);
}

#endif
