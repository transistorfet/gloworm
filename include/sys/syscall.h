
#ifndef _SYS_UNISTD_H
#define _SYS_UNISTD_H

#include <errno.h>
#include <asm/syscall.h>

#define SYS_TEST	0
#define SYS_EXIT	1
#define SYS_FORK	2
#define SYS_READ	3
#define SYS_WRITE	4
#define SYS_OPEN	5
#define SYS_CLOSE	6
#define SYS_WAITPID	7
#define SYS_CREAT	8
#define SYS_LINK	9
#define SYS_UNLINK	10
#define SYS_EXECVE	11
#define SYS_CHDIR	12
#define SYS_TIME	13
#define SYS_MKNOD	14
#define SYS_CHMOD	15
#define SYS_CHOWN	16
#define SYS_BRK		17
#define SYS_STAT	18
#define SYS_LSEEK	19
#define SYS_GETPID	20
#define SYS_MOUNT	21
#define SYS_UMOUNT	22
#define SYS_SETUID	23
#define SYS_GETUID	24
#define SYS_STIME	25
#define SYS_PTRACE	26
#define SYS_ALARM	27
#define SYS_FSTAT	28
#define SYS_PAUSE	29
#define SYS_SIGRETURN	30
#define SYS_SIGACTION	31
#define SYS_ACCESS	32
#define SYS_SYNC	33
#define SYS_KILL	34
#define SYS_RENAME	35
#define SYS_MKDIR	36
#define SYS_RMDIR	37
#define SYS_GETCWD	38
#define SYS_DUP2	39
#define SYS_PIPE	40
#define SYS_IOCTL	41
#define SYS_FCNTL	42
#define SYS_READDIR	43
#define SYS_GETPPID	44
#define SYS_SYMLINK	45
#define SYS_GETPGID	46
#define SYS_SETPGID	47
#define SYS_GETSID	48
#define SYS_SETSID	49
#define SYS_UMASK	50
#define SYS_SBRK	51
#define SYS_SELECT	52

#define SYS_SOCKET	53
#define SYS_SOCKETPAIR	54
#define SYS_CONNECT	55
#define SYS_BIND	56
#define SYS_LISTEN	57
#define SYS_ACCEPT	58
#define SYS_SHUTDOWN	59
#define SYS_SEND	60
#define SYS_SENDTO	61
#define SYS_SENDMSG	62
#define SYS_RECV	63
#define SYS_RECVFROM	64
#define SYS_RECVMSG	65
#define SYS_GETSOCKOPT	66
#define SYS_SETSOCKOPT	67

// TODO remove this after testing
#define SYS_EXECBUILTIN	68

// TODO reorganize these, and maybe follow linux's abi, if it's already pretty close, to make it easier to port applications?
#define SYS_CLONE	69
#define SYS_GETTID	70

#endif
