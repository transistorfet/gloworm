
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/sched.h>

#define STACK_SIZE	1024

int thread(void *arg);

int main(int argc, char **argv)
{
	int pid;
	char *stack = malloc(STACK_SIZE);

	pid = clone(thread, stack + STACK_SIZE, CLONE_VM, (void *) 0x55AA);
	if (pid) {
		printf("child %d\n", pid);
		return 0;
	} else {
		printf("I am detached\n");

	}

	return 0;
}

int thread(void *arg)
{
	printf("I'm a little thread: %x\n", (unsigned int) arg);

	return 0;
}
