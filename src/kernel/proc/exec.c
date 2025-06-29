
#include <stddef.h>
#include <string.h>
#include <sys/param.h>
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

char *copy_exec_args(struct memory_map *map, char *stack, const char *const argv[], const char *const envp[])
{
	int argc, envc;

	if (envp) {
		copy_string_array(&stack, &envc, envp);
		map->envp = (const char *const *) stack;
	} else {
		map->envp = NULL;
	}

	if (argv) {
		copy_string_array(&stack, &argc, argv);
		map->argv = (const char *const *) stack;
	} else {
		map->argv = NULL;
	}

	stack -= sizeof(char **);
	*((char ***) stack) = (char **) map->envp;
	stack -= sizeof(char **);
	*((char ***) stack) = (char **) map->argv;
	stack -= sizeof(int);
	*((int *) stack) = argc;

	return stack;
}

void *exec_initialize_stack_entry(struct memory_map *map, void *stack_pointer, void *entry)
{
	//PUSH_STACK(stack_pointer, void *) = _exit;
	stack_pointer = ((char *) stack_pointer) - sizeof(void *);
	*((void **) stack_pointer) = _exit;
	stack_pointer = create_context(stack_pointer, entry, NULL, NULL);
	return stack_pointer;
}

void *exec_initialize_stack_with_args(struct memory_map *map, void *stack_pointer, void *entry, const char *const argv[], const char *const envp[])
{
	// TODO this will be done to the user stack (which is the same as the kernel stack if no user mode)
	stack_pointer = copy_exec_args(map, stack_pointer, argv, envp);
	//PUSH_STACK(stack_pointer, void *) = _exit;
	stack_pointer = ((char *) stack_pointer) - sizeof(void *);
	*((void **) stack_pointer) = _exit;

	// TODO this will be done to the kernel stack always
	stack_pointer = create_context(stack_pointer, entry, NULL, NULL);
	return stack_pointer;
}

int exec_reset_and_initialize_stack(struct process *proc, struct memory_map *map, void *entry, const char *const argv[], const char *const envp[])
{
	int error;

	// Reset sbrk to the start of the heap
	error = memory_map_move_sbrk(map, -1 * (map->sbrk - map->heap_start));
	if (error < 0) {
		return error;
	}

	// Initialize the stack pointer first, so that the check in memory_map_move_sbrk will pass
	proc->sp = exec_initialize_stack_with_args(map, (char *) map->stack_end, entry, argv, envp);

	return 0;
}

int exec_alloc_new_stack(struct process *proc, struct memory_map *map, int stack_size, void *entry, const char *const argv[], const char *const envp[])
{
	int error = 0;

	error = memory_map_insert_heap_stack(map, stack_size);
	if (error < 0)
		return error;

	return exec_reset_and_initialize_stack(proc, map, entry, argv, envp);
}
