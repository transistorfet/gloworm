
#include <string.h>


int strlen(const char *str)
{
	register int i = 0;

	while (str[i++] != '\0')
		/* intentionally empty */;
	return(i);
}
