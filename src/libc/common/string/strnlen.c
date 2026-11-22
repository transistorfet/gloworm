
#include <string.h>


size_t strnlen(const char *str, size_t maxlen)
{
	register volatile int i = 0;

	for (; i < maxlen && str[i] != '\0'; i++) {}
	return i;
}

