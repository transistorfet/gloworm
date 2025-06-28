
#ifndef _M68K_SYSCALL_TABLE_H
#define _M68K_SYSCALL_TABLE_H

#include <kernel/api.h>
#include <kernel/printk.h>


#define SYSCALL_MAX	128

#define __SYSCALL(number, label)	label

extern int __do_fork(void);
extern int __do_clone(void);
extern int __do_sigreturn(void);

void test(void) { printk("It's a test!\n"); }

void *syscall_table[SYSCALL_MAX] = {
	__SYSCALL(0, test),
	__SYSCALL(1, do_exit),
	__SYSCALL(2, __do_fork),
	__SYSCALL(3, do_read),
	__SYSCALL(4, do_write),
	__SYSCALL(5, do_open),
	__SYSCALL(6, do_close),
	__SYSCALL(7, do_waitpid),
	__SYSCALL(8, do_creat),
	__SYSCALL(9, do_link),
	__SYSCALL(10, do_unlink),
	__SYSCALL(11, do_exec),
	__SYSCALL(12, do_chdir),
	__SYSCALL(13, do_time),
	__SYSCALL(14, do_mknod),
	__SYSCALL(15, do_chmod),
	__SYSCALL(16, do_chown),
	__SYSCALL(17, do_brk),
	__SYSCALL(18, do_stat),
	__SYSCALL(19, do_lseek),
	__SYSCALL(20, do_getpid),
	__SYSCALL(21, do_mount),
	__SYSCALL(22, do_umount),
	__SYSCALL(23, do_setuid),
	__SYSCALL(24, do_getuid),
	__SYSCALL(25, do_stime),
	__SYSCALL(26, test),		// 26 = ptrace, not yet implemented
	__SYSCALL(27, do_alarm),
	__SYSCALL(28, do_fstat),
	__SYSCALL(29, do_pause),
	__SYSCALL(30, __do_sigreturn),
	__SYSCALL(31, do_sigaction),
	__SYSCALL(32, do_access),
	__SYSCALL(33, do_sync),
	__SYSCALL(34, do_kill),
	__SYSCALL(35, do_rename),
	__SYSCALL(36, do_mkdir),
	__SYSCALL(37, test),		// 37 = rmdir, not yet implemented
	__SYSCALL(38, do_getcwd),
	__SYSCALL(39, do_dup2),
	__SYSCALL(40, do_pipe),
	__SYSCALL(41, do_ioctl),
	__SYSCALL(42, do_fcntl),
	__SYSCALL(43, do_readdir),
	__SYSCALL(44, do_getppid),
	__SYSCALL(45, test),		// 45 = symlink, not yet implemented
	__SYSCALL(46, do_getpgid),
	__SYSCALL(47, do_setpgid),
	__SYSCALL(48, do_getsid),
	__SYSCALL(49, do_setsid),
	__SYSCALL(50, do_umask),
	__SYSCALL(51, do_sbrk),
	__SYSCALL(52, do_select),

	__SYSCALL(53, do_socket),
	__SYSCALL(54, do_socketpair),
	__SYSCALL(55, do_connect),
	__SYSCALL(56, do_bind),
	__SYSCALL(57, do_listen),
	__SYSCALL(58, do_accept),
	__SYSCALL(59, do_shutdown),
	__SYSCALL(60, do_send),
	__SYSCALL(61, do_sendto),
	__SYSCALL(62, do_sendmsg),
	__SYSCALL(63, do_recv),
	__SYSCALL(64, do_recvfrom),
	__SYSCALL(65, do_recvmsg),
	__SYSCALL(66, do_getsockopt),
	__SYSCALL(67, do_setsockopt),

	__SYSCALL(68, do_execbuiltin),
	__SYSCALL(69, __do_clone),
	__SYSCALL(70, do_gettid),
};

#endif

