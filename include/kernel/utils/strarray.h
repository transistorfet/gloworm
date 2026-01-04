
#ifndef _UTILS_STRARRAY_H
#define _UTILS_STRARRAY_H

#define EXEC_ARGS_COUNT_MAX	32
#define EXEC_ARGS_LENGTH_MAX	1024

#include <stdint.h>

#include <kernel/utils/iovec.h>
#include <kernel/utils/usercopy.h>

#include <asm/addresses.h>

#define FROM_USER	1
#define FROM_KERNEL	0

struct string_array {
	int len;
	int used;
	uintptr_t *strings;
	char buffer[EXEC_ARGS_LENGTH_MAX];
};

int string_array_copy(struct string_array *array, const char __user *const src_words[], int from_user);
void string_array_add_offset(struct string_array *array, uintptr_t offset);
void string_array_remove_offset(struct string_array *array, uintptr_t offset);
int string_array_copy_to_iter(struct string_array *array, virtual_address_t vaddr, struct iovec_iter *iter);

static inline const char *string_array_get(struct string_array *array, int i)
{
	return &array->buffer[array->strings[i]];
}

#endif

