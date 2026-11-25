
#include <stdio.h>
#include <unistd.h>
#include <attributes.h>
#include "file.h"

size_t fread(void *ptr, size_t size, size_t count, __unused FILE *stream)
{
	register char *str = (char *) ptr;

	size = count * size;
	for (size_t i = 0; i < size; i++) {
		str[i] = getchar();
	}
	return count;
}

