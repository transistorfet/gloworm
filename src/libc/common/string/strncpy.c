
#include <string.h>


char *strncpy(char *dest, const char *src, unsigned long max)
{
	char *ret = dest;

	for (; max > 0; max--)
		*dest++ = (*src != '\0') ? *src++ : 0;
	if (max > 0)
		*dest = '\0';
	return ret;
}

