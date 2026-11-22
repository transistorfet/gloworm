
#include <string.h>


size_t strlen(const char *str)
{
	register volatile int i = 0;

	for (; str[i] != '\0'; i++) {}
	return i;
}

