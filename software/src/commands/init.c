
#include <unistd.h>
#include <kernel/syscall.h>

int init_task()
{
	// Open stdin
	int fd = open("tty", O_RDONLY, 0);
	if (fd < 0) {
		printk("Error opening file tty %d\n", fd);
		return NULL;
	}

	// Open stdout
	fd = open("tty", O_WRONLY, 0);
	if (fd < 0) {
		printk("Error opening file tty %d\n", fd);
		return NULL;
	}

	// Open stderr
	fd = open("tty", O_WRONLY, 0);
	if (fd < 0) {
		printk("Error opening file tty %d\n", fd);
		return NULL;
	}

	int pid, status;
	char *argv[2] = { NULL };
	char *envp[2] = { NULL };

 	pid = fork();
	if (!pid) {
		extern void sh_task();
		status = SYSCALL3(SYS_EXECBUILTIN, (int) sh_task, (int) argv, (int) envp);
		// The exec() system call will only return if an error occurs
		printf("Failed to execute %s: %d\n", argv[1], status);
		exit(-1);
	}

	while (1) {
		wait(&status);
		printf("Process %d exited with %d\n", pid, status);
	}
}


