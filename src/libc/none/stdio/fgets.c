
#include <stdio.h>
#include <string.h>
#include <attributes.h>

char *fgets(char *str, int max, __unused FILE *stream)
{
	int i = 0;

	for (; i < max - 1; i++) {
		str[i] = getchar();
		if (str[i] == '\n') {
			i += 1;
			break;
		}
	}
	str[i] = '\0';
	return str;
}

