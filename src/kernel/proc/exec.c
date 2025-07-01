
#include <stddef.h>
#include <string.h>
#include <sys/param.h>
#include <asm/context.h>
#include <kernel/proc/exec.h>
#include <kernel/proc/memory.h>
#include <kernel/arch/context.h>
#include <kernel/proc/process.h>


int copy_string_array(char **stack, int *count, const char *const arr[])
{
	int len = 0;
	*count = 0;

	for (int i = 0; arr[i] != NULL; i++) {
		// String, line terminator, and a pointer
		len += sizeof(char *) + strlen(arr[i]) + 1;
		*count += 1;
	}
	len += sizeof(char *);

	// Align to the nearest word
	len = roundup(len, 2);

	char **dest_arr = (char **) (*stack - len);
	char *buffer = ((char *) dest_arr) + (sizeof(char *) * (*count + 1));
	*stack = (char *) dest_arr;

	int i = 0, j = 0;
	for (; j < *count; j++) {
		dest_arr[j] = &buffer[i];
		strcpy(dest_arr[j], arr[j]);
		i += strlen(arr[j]) + 1;
	}
	dest_arr[j] = NULL;

	return 0;
}

char *copy_exec_args(struct memory_map *map, char *user_sp, const char *const argv[], const char *const envp[])
{
	int argc, envc;

	if (envp) {
		copy_string_array(&user_sp, &envc, envp);
		map->envp = (const char *const *) user_sp;
	} else {
		map->envp = NULL;
	}

	if (argv) {
		copy_string_array(&user_sp, &argc, argv);
		map->argv = (const char *const *) user_sp;
	} else {
		map->argv = NULL;
	}

	user_sp -= sizeof(char **);
	*((char ***) user_sp) = (char **) map->envp;
	user_sp -= sizeof(char **);
	*((char ***) user_sp) = (char **) map->argv;
	user_sp -= sizeof(int);
	*((int *) user_sp) = argc;

	return user_sp;
}

void exec_initialize_stack_with_args(struct process *proc, void *stack_pointer, void *entry, const char *const argv[], const char *const envp[])
{
	// TODO this will be done to the user stack (which is the same as the kernel stack if no user mode)
	stack_pointer = copy_exec_args(proc->map, stack_pointer, argv, envp);

	arch_reinit_task_info(proc, stack_pointer, entry);
}

