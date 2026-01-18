
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
		// NOTE this assumes the user stack's virtual address is the active MMU map, so memcpy_to_user() works
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
		// NOTE this assumes the user stack's virtual address is the active MMU map, so memcpy_to_user() works
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

int exec_initialize_kernel_stack_with_args(struct process *proc, virtual_address_t stack_pointer, void *entry, struct string_array *argv, struct string_array *envp)
{
	struct iovec_iter iter;

	//iovec_iter_init_user_buf(&iter, (char *) user_sp, argv->used);
	iovec_iter_init_kernel_buf(&iter, (char *) stack_pointer - PAGE_SIZE, PAGE_SIZE);
	iovec_iter_seek(&iter, 0, SEEK_END);

	// TODO this is probably not a good place for this, but I'm not sure where else it could go
	if (current_proc != proc) {
		arch_extended_switch_context(NULL, proc);
	}

	stack_pointer = copy_exec_args(proc->map, stack_pointer - PAGE_SIZE, &iter, argv, envp);

	arch_add_process_context(proc, (char *) stack_pointer, entry);
	return 0;
}

int exec_initialize_user_stack_with_args(struct process *proc, virtual_address_t stack_pointer, void *entry, struct string_array *argv, struct string_array *envp)
{
	struct iovec_iter iter;

	iovec_iter_init_user_buf(&iter, (char *) stack_pointer - PAGE_SIZE, PAGE_SIZE);
	//iovec_iter_init_kernel_buf(&iter, (char *) stack_pointer - PAGE_SIZE, PAGE_SIZE);
	iovec_iter_seek(&iter, 0, SEEK_END);


	/*
	#define IOVEC_PAGES(size)		(((size) / PAGE_SIZE) > 0 ? ((size) / PAGE_SIZE) : 1)
	#define PROC_ARG_PAGES			IOVEC_PAGES(1024)
	int error;
	struct kvec kvec[PROC_ARG_PAGES];
	struct iovec_iter iter;

	error = memory_map_load_pages_into_kvec(proc->map, kvec, PROC_ARG_PAGES, stack_pointer - (PAGE_SIZE * PROC_ARG_PAGES), IOVEC_WRITE);
	if (error < 0)
		return error;
	iovec_iter_init_kvec(&iter, kvec, PROC_ARG_PAGES);
	//iovec_iter_init_kernel_buf(&iter, (char *) stack_pointer - PAGE_SIZE, PAGE_SIZE);
	iovec_iter_seek(&iter, 0, SEEK_END);
	*/


	// TODO this is probably not a good place for this, but I'm not sure where else it could go
	if (current_proc == proc) {
		arch_extended_switch_context(NULL, proc);
	}

	stack_pointer = copy_exec_args(proc->map, stack_pointer - PAGE_SIZE, &iter, argv, envp);

	arch_add_process_context(proc, (char *) stack_pointer, entry);
	return 0;
}

