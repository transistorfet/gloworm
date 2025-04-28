 
#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>

int getchar()
{
	unsigned char ch;
	read(STDIN_FILENO, &ch, 1);
	return ch;
}

