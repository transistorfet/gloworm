
#include <stddef.h>
#include <string.h>
#include <sys/param.h>

#include <asm/context.h>
#include <asm/addresses.h>

#include <kernel/proc/exec.h>
#include <kernel/proc/memory.h>
#include <kernel/arch/context.h>
#include <kernel/proc/process.h>
#include <kernel/utils/strarray.h>


static virtual_address_t copy_exec_args(struct memory_map *map, virtual_address_t address_offset, struct iovec_iter *iter, struct string_array *argv, struct string_array *envp)
{
	int result;
	offset_t position;

	position = iovec_iter_seek(iter, 0, SEEK_CUR);
	if (envp) {
		position -= envp->used;
		position = iovec_iter_seek(iter, position, SEEK_SET);
		result = string_array_copy_to_iter(envp, address_offset + position, iter);
		if (result < 0)
			return result;
		map->envp = (const char *const *) (address_offset + position);
	} else {
		map->envp = NULL;
	}

	if (argv) {
		position -= argv->used;
		position = iovec_iter_seek(iter, position, SEEK_SET);
		result = string_array_copy_to_iter(argv, address_offset + position, iter);
		if (result < 0)
			return result;
		map->argv = (const char *const *) (address_offset + position);
	} else {
		map->argv = NULL;
	}
	position = iovec_iter_seek(iter, position, SEEK_SET);

	result = iovec_iter_push_back(iter, &map->envp, sizeof(char **));
	if (result < 0)
		return result;
	result = iovec_iter_push_back(iter, &map->argv, sizeof(char **));
	if (result < 0)
		return result;
	result = iovec_iter_push_back(iter, &argv->len, sizeof(int));
	if (result < 0)
		return result;

	position = iovec_iter_seek(iter, 0, SEEK_CUR);
	return address_offset + position;
}

int exec_initialize_stack_with_args(struct process *proc, virtual_address_t stack_pointer, void *entry, struct string_array *argv, struct string_array *envp)
{
	int result;
	struct kvec kvec[4];
	struct iovec_iter iter;
	virtual_address_t buffered_size = PAGE_SIZE * 2;

	result = iovec_iter_load_pages_iter(proc->map, &iter, kvec, 4, stack_pointer - buffered_size, buffered_size, 1);
	if (result < 0) {
		return result;
	}

	result = iovec_iter_seek(&iter, 0, SEEK_END);
	if (result < 0) {
		return result;
	}

	if (current_proc != proc) {
		arch_extended_switch_context(NULL, proc);
	}

	stack_pointer = copy_exec_args(proc->map, stack_pointer - buffered_size, &iter, argv, envp);

	arch_add_process_context(proc, (char *) stack_pointer, entry);
	return 0;
}

