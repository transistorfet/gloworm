
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
	char *data;

	data = malloc(100);
	strcpy(data, "Hey there");

	printf("%s\n", data);
	return(0);
}

