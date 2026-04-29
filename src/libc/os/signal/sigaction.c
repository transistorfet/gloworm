
#include <signal.h>
#include <sys/syscall.h>

extern void _sigreturn(void);

int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact)
{
	((struct sigaction *) act)->sa_restorer = _sigreturn;
	((struct sigaction *) act)->sa_flags |= SA_RESTORER;
	return SYSCALL3(SYS_SIGACTION, (int) signum, (int) act, (int) oldact);
}

