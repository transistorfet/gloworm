 
#include <stdio.h>

#include <unistd.h>
#include <kernel/syscall.h>


int main(int argc, char **argv)
{
	//const char *msg = "Hello!\n";

	printf("%d %s\n", argc, argv[0]);

	//while (1) {
		puts("Hello there, World!");
		//write(0, msg, 14);
		//putchar('#');
		//SYSCALL1(SYS_PUTCHAR, '!');
		//SYSCALL3(SYS_WRITE, 0, "Hello, World!\n", 14);
		//SYSCALL3(SYS_WRITE, 0, msg, 14);


		//for (int i = 0; i < 1000000; i++) {
		//	asm volatile("");
		//}
	//}

	return 0;
}

