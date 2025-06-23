
#ifndef UNISTD_H
#define UNISTD_H

#include <stddef.h>
#include <dirent.h>
#include <sys/types.h>

#define F_OK		0	// Test if file exists
#define X_OK		1	// Test if file is executable
#define W_OK		2	// Test if file is writable
#define R_OK		4	// Test if file is readable

#define	STDIN_FILENO	0	// Standard input
#define	STDOUT_FILENO	1	// Standard output
#define	STDERR_FILENO	2	// Standard error

// WHENCE argument to lseek
#ifndef	STDIO_H			// These same definitions are in stdio.h for fseek
#define SEEK_SET	0	// Seek relative to the beginning of file
#define SEEK_CUR	1	// Seek relative to the current position
#define SEEK_END	2	// Seek relative to the end of file
#endif

#define PIPE_READ_FD	0
#define PIPE_WRITE_FD	1

struct stat;

// Syscalls

pid_t fork();
pid_t clone(int (*fn)(void *), void *stack, int flags, void *arg);
void exit(int status);
int execve(const char *path, char *const argv[], char *const envp[]);
pid_t wait(int *status);
pid_t waitpid(pid_t pid, int *status, int options);
int kill(pid_t pid, int sig);
unsigned int alarm(unsigned int seconds);
int pause();
int brk(void *addr);
void *sbrk(intptr_t increment);
pid_t gettid();
pid_t getpid();
pid_t getppid();
pid_t getpgid(pid_t pid);
int setpgid(pid_t pid, pid_t pgid);
pid_t getsid(pid_t pid);
pid_t setsid(void);
uid_t getuid();
int setuid(uid_t uid);

int mkdir(const char *path, mode_t mode);
char *getcwd(char *buf, size_t size);
int rename(const char *oldpath, const char *newpath);

int mknod(const char *path, mode_t mode, device_t dev);
int creat(const char *path, mode_t mode);
int open(const char *path, int flags, mode_t mode);
int close(int fd);
ssize_t read(int fd, void *buf, size_t nbytes);
ssize_t write(int fd, const void *buf, size_t nbytes);
int readdir(int fd, struct dirent *dir);
int ioctl(int fd, unsigned int request, void *argp);

int access(const char *path, int mode);
int chdir(const char *path);
int chown(const char *path, uid_t owner, gid_t group);
int chmod(const char *path, mode_t mode);
int stat(const char *path, struct stat *statbuf);
int fstat(int fd, struct stat *statbuf);
int link(const char *oldpath, const char *newpath);
int symlink(const char *target, const char *linkpath);
int unlink(const char *path);
offset_t lseek(int fd, offset_t offset, int whence);
void sync(void);
mode_t umask(mode_t mask);

int pipe(int pipefd[2]);
int dup2(int oldfd, int newfd);

//// Library Functions ////

extern char **environ;
extern char *optarg;
extern int optind, opterr, optopt;

char *getenv(const char *name);
int getopt(int argc, char * const argv[], const char *optstring);

pid_t tcgetpgrp(int fd);
int tcsetpgrp(int fd, pid_t pgrp);

#endif
