
#include <stdio.h>
#include <unistd.h>
#include <attributes.h>
#include "file.h"

size_t fwrite(const void *ptr, size_t size, size_t count, __unused FILE *stream)
{
	register const char *str = (const char *) ptr;

	size = count * size;
	for (size_t i = 0; i < size; i++) {
		putchar(str[i]);
	}
	return count;
}

