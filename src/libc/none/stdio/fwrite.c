
#include <stdio.h>
#include <unistd.h>
#include "file.h"

size_t fwrite(const void *ptr, size_t size, size_t count, FILE *stream)
{
	register const char *str = (const char *) str;

	size = count * size;
	for (int i = 0; i < size; i++)
		putchar(str[i]);
	return count;
}

