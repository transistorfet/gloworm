
#ifndef _KERNEL_API_H
#define _KERNEL_API_H

#include <time.h>
#include <dirent.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <sys/select.h>
#include <sys/socket.h>

#include <kernel/utils/usercopy.h>


void init_syscall(void);
void do_syscall(void);

// Processes
extern void do_exit(int exitcode);
extern pid_t do_fork(void);
extern pid_t do_clone(void);
extern int do_exec(const char __user *path, const char __user *const argv[], const char __user *const envp[]);
extern pid_t do_waitpid(pid_t pid, int __user *status, int options);
extern int do_kill(pid_t pid, int sig);
extern int do_sigreturn(void);
extern int do_sigaction(int signum, const struct sigaction __user *act, struct sigaction __user *oldact);
extern unsigned int do_alarm(unsigned int seconds);
extern int do_pause(void);
extern int do_brk(void __user *addr);
extern void *do_sbrk(intptr_t increment);
extern pid_t do_gettid(void);
extern pid_t do_getpid(void);
extern pid_t do_getppid(void);
extern pid_t do_getpgid(pid_t pid);
extern int do_setpgid(pid_t pid, pid_t pgid);
extern pid_t do_getsid(pid_t pid);
extern pid_t do_setsid(void);
extern uid_t do_getuid(void);
extern int do_setuid(uid_t uid);

// Files & Directories
extern size_t do_read(int fd, char __user *buf, size_t nbytes);
extern size_t do_write(int fd, const char __user *buf, size_t nbytes);
extern int do_creat(const char __user *path, mode_t mode);
extern int do_open(const char __user *path, int oflags, mode_t mode);
extern int do_close(int fd);
extern int do_readdir(int fd, struct dirent __user *dir);
extern int do_ioctl(int fd, unsigned int request, void *argp);
extern int do_fcntl(int fd, int cmd, void __user *argp);
extern int do_mknod(const char __user *path, mode_t mode, device_t dev);
extern int do_link(const char __user *oldpath, const char __user *newpath);
extern int do_unlink(const char __user *path);
extern int do_mkdir(const char __user *path, mode_t mode);
extern char *do_getcwd(char __user *buf, size_t size);
extern int do_rename(const char __user *oldpath, const char __user *newpath);
extern int do_chdir(const char __user *path);
extern int do_access(const char __user *path, int mode);
extern int do_chown(const char __user *path, uid_t owner, gid_t group);
extern int do_chmod(const char __user *path, int mode);
extern int do_stat(const char __user *path, struct stat __user *statbuf);
extern int do_fstat(int fd, struct stat __user *statbuf);
extern int do_lseek(int fd, offset_t offset, int whence);
extern int do_pipe(int pipefd[2]);
extern int do_dup2(int oldfd, int newfd);
extern mode_t do_umask(mode_t mask);
extern int do_mount(const char __user *source, const char __user *target, struct mount_opts __user *opts);
extern int do_umount(const char __user *source);
extern int do_sync(void);
extern int do_select(int nfds, fd_set __user *readfds, fd_set __user *writefds, fd_set __user *exceptfds, struct timeval __user *timeout);

// Time
extern time_t do_time(time_t __user *t);
extern int do_stime(const time_t __user *t);

// Sockets
extern int do_socket(int domain, int type, int protocol);
extern int do_socketpair(int domain, int type, int protocol, int fds[2]);
extern int do_connect(int fd, const struct sockaddr __user *addr, socklen_t len);
extern int do_bind(int fd, const struct sockaddr __user *addr, socklen_t len);
extern int do_listen(int fd, int n);
extern int do_accept(int fd, struct sockaddr __user *addr, socklen_t __user *addr_len);
extern int do_shutdown(int fd, int how);
extern ssize_t do_send(int fd, const void __user *buf, size_t n, int flags);
extern ssize_t do_sendto(int fd, const void __user *buf, size_t n, int flags, int opts[2]);
extern ssize_t do_sendmsg(int fd, const struct msghdr __user *message, int flags);
extern ssize_t do_recv(int fd, void __user *buf, size_t n, int flags);
extern ssize_t do_recvfrom(int fd, void __user *buf, size_t n, int flags, int opts[2]);
extern ssize_t do_recvmsg(int fd, struct msghdr __user *message, int flags);
extern int do_getsockopt(int fd, int level, int optname, void __user *optval, socklen_t __user *optlen);
extern int do_setsockopt(int fd, int level, int optname, const void __user *optval, socklen_t optlen);

#endif
