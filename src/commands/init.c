
#include <stdio.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioc_tty.h>
#include <sys/syscall.h>

#include "prototype.h"

int MAIN(init_task)()
{
	// Open stdin
	int fd = open("/dev/tty0", O_RDWR, 0);
	if (fd < 0) {
		printf("Error opening file tty %d\n", fd);
		return NULL;
	}
	// Open stdout
	dup2(fd, 1);
	// Open stderr
	dup2(fd, 2);

	// TODO temporary, for testing
	int test_files();
	int test_dirs();
	//test_files();
	//test_dirs();


	int pid, status;
	char *argv[3] = { NULL };
	char *envp[2] = { NULL };

 	pid = fork();
	if (!pid) {
		// TODO create a new session
		setpgid(0, 0);
		pid_t fgpid = getpgid(0);
		ioctl(STDOUT_FILENO, TIOCSPGRP, &fgpid);

		argv[0] = "/bin/sh";

		#if defined(CONFIG_SHELL_IN_KERNEL) && defined(IN_KERNEL)

		// Run the builtin shell if compiled inside the kernel
		extern void sh_task();
		argv[0] = "sh";
		status = SYSCALL3(SYS_EXECBUILTIN, (int) sh_task, (int) argv, (int) envp);

		#else

		// Run a separate shell to run the rc script
		argv[1] = "/etc/rc";
		if (!access(argv[1], X_OK)) {
			if (!fork()) {
				execve(argv[0], argv, envp);
				exit(0);
			} else {
				wait(&status);
			}
		}
		argv[1] = NULL;

		// Run the shell
		status = execve(argv[0], argv, envp);

		#endif

		// The exec() system call will only return if an error occurs
		printf("Failed to execute %s: %d\n", argv[1], status);
		exit(-1);
	}

	while (1) {
		pid = wait(&status);
		printf("Process %d exited with %d\n", pid, status);
	}
}


