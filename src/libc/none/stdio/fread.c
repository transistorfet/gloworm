
#include <stdio.h>
#include <unistd.h>
#include "file.h"

size_t fread(void *ptr, size_t size, size_t count, FILE *stream)
{
	register char *str = (char *) str;

	size = count * size;
	for (int i = 0; i < size; i++)
		str[i] = getchar();
	return count;
}

