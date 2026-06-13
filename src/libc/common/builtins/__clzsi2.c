
#include <stdio.h>
int __clzsi2(unsigned int a)
{
	int index = 32;

	if ((a & 0xFFFF0000) == 0) {
		index -= 16;
		a <<= 16;
	}
	if ((a & 0xFF000000) == 0) {
		index -= 8;
		a <<= 8;
	}
	if ((a & 0xF0000000) == 0) {
		index -= 4;
		a <<= 4;
	}
	if ((a & 0xC0000000) == 0) {
		index -= 2;
		a <<= 2;
	}
	if ((a & 0x80000000) == 0) {
		index -= 1;
	}
	return 32 - index;
}

