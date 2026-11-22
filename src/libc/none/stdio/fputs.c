
#include <stdio.h>
#include <string.h>
#include <attributes.h>

int fputs(const char *str, __unused FILE *stream)
{
	for (; *str != '\0'; str++) {
		putchar(*str);
	}
	return 0;
}

