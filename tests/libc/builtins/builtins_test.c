
#include <assert.h>
#include <stdio.h>
#include <string.h>

extern int __ctzsi2(unsigned int a);
extern int __clzsi2(unsigned int a);
extern int __ffsdi2(unsigned long a);

int test_ctz_clz(void)
{
	assert(__ctzsi2(0x100) == 8);
	assert(__ctzsi2(0x1234) == 2);
	assert(__ctzsi2(0x001) == 0);
	assert(__ctzsi2(0xFFF) == 0);
	assert(__ctzsi2(0xFFFF0000) == 16);
	assert(__ctzsi2(0xF0000000) == 28);

	assert(__ffsdi2(0x100) == 8);
	assert(__ffsdi2(0x1234) == 2);
	assert(__ffsdi2(0x001) == 0);
	assert(__ffsdi2(0xFFF) == 0);
	assert(__ffsdi2(0xFFFF0000) == 16);
	assert(__ffsdi2(0xF0000000) == 28);

	assert(__clzsi2(0x100) == 23);
	assert(__clzsi2(0x1234) == 19);
	assert(__clzsi2(0x001) == 31);
	assert(__clzsi2(0xFFF) == 20);
	assert(__clzsi2(0xFFFF0000) == 0);
	assert(__clzsi2(0xF0000000) == 0);

	return 0;
}

#define run(test) \
	printf("Running %s\n", #test); \
	assert(test() == 0);

int main(void)
{
	run(test_ctz_clz);

	printf("%s tests passed\n", __FILE_NAME__);

	return 0;
}
