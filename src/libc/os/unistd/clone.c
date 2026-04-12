
#include <unistd.h>
#include <sys/syscall.h>
#include <bits/macros.h>

struct user_clone_args {
	int (*fn)(void *);
	void *arg;
};

pid_t clone(int (*fn)(void *), void *stack, int flags, void *arg)
{
	if (!stack) {

		return SYSCALL2(SYS_CLONE, (unsigned long) stack, flags);

	} else {
		struct user_clone_args *user_args;

		stack = (char *) stack - sizeof(struct user_clone_args);
		user_args = (struct user_clone_args *) stack;
		user_args->fn = fn;
		user_args->arg = arg;

		pid_t result = SYSCALL2(SYS_CLONE, (unsigned long) stack, flags);
		if (result)
			return result;

		STACK_POINTER(user_args);
		int return_code = user_args->fn(user_args->arg);
		exit(return_code);
	}
}

