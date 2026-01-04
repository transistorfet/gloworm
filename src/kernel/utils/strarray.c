
#include <sys/param.h>

#include <asm/addresses.h>

#include <kernel/utils/iovec.h>
#include <kernel/utils/usercopy.h>
#include <kernel/utils/strarray.h>


int string_array_copy(struct string_array *array, const char __user *const src_words[], int from_user)
{
	size_t i;
	size_t len = 0;
	size_t used = 0;
	size_t str_len;
	const char *str;

	// Count the arguments
	if (from_user) {
		while (1) {
			str = (char *) get_user_uintptr((void *) &src_words[len]);
			if (!str) {
				break;
			}
			len += 1;
		}
	} else {
		for (; src_words[len]; len++) {}
	}

	// Set up the array of word offsets inside the buffer
	array->len = len;
	array->strings = (uintptr_t *) &array->buffer[0];
	used = (len + 1) * sizeof(uintptr_t);

	// Copy the strings into the remaining buffer, and record the offsets
	for (i = 0; i < len && used < EXEC_ARGS_LENGTH_MAX; i++) {
		array->strings[i] = used;
		if (from_user) {
			str = (char *) get_user_uintptr((void *) &src_words[i]);
			str_len = strncpy_from_user(&array->buffer[used], str, EXEC_ARGS_LENGTH_MAX - used);
		} else {
			str_len = strlen(src_words[i]);
			strncpy(&array->buffer[used], src_words[i], EXEC_ARGS_LENGTH_MAX - used);
		}
		used += str_len + 1;
	}
	array->used = roundup(used, sizeof(uintptr_t));
	return len;
}

void string_array_add_offset(struct string_array *array, uintptr_t offset)
{
	size_t i;

	for (i = 0; i < array->len; i++) {
		array->strings[i] += offset;
	}
}

void string_array_remove_offset(struct string_array *array, uintptr_t offset)
{
	size_t i;

	for (i = 0; i < array->len; i++) {
		array->strings[i] -= offset;
	}
}

int string_array_copy_to_iter(struct string_array *array, virtual_address_t vaddr, struct iovec_iter *iter)
{
	// TODO should this insert it into the end of the iter, or should it put it at the start and rely on it being set up to actually be the end?

	if (iovec_iter_length(iter) < array->used) {
		return -1;
	}

	string_array_add_offset(array, (uintptr_t) vaddr);
	memcpy_into_iter(iter, array->buffer, array->used);
	string_array_remove_offset(array, (uintptr_t) vaddr);

	return 0;
}

