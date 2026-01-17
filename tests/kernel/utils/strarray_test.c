
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <kernel/utils/iovec.h>
#include <kernel/utils/strarray.h>


int main(void)
{
	int num_args = 3;
	struct string_array array;
	char *args[] = { "arg0", "arg1", "arg2", NULL };

	string_array_copy(&array, (const char **) args, FROM_KERNEL);
	assert(array.len == num_args);

	assert(!strcmp(&array.buffer[array.strings[0]], "arg0"));
	assert(!strcmp(&array.buffer[array.strings[1]], "arg1"));
	assert(!strcmp(&array.buffer[array.strings[2]], "arg2"));
	assert(array.used == (sizeof(uintptr_t) * (num_args + 1) + (5 * 3) + 1));	// the 1 at the end here is the padding that will be added to make it an even number of uintptr_t


	#define BUFFER_MAX	64
	char buffer[BUFFER_MAX];
	// Initialize the buffer with a known value, so it's clear what's been modified by the function to test
	for (int i = 0; i < BUFFER_MAX; i++) {
		buffer[i] = 0xff;
	}
	struct iovec_iter iter;

	iovec_iter_init_kernel_buf(&iter, buffer, BUFFER_MAX);
	string_array_copy_to_iter(&array, (uintptr_t) buffer, &iter);
	printk_dump_bytes((uint8_t *) buffer, BUFFER_MAX);

	int arg0_start = sizeof(uintptr_t) * (num_args + 1);
	assert(!strcmp(&buffer[arg0_start], "arg0"));
	assert(!strcmp(&buffer[arg0_start + 5], "arg1"));
	assert(!strcmp(&buffer[arg0_start + 10], "arg2"));

	assert(((uintptr_t *) buffer)[0] == (uintptr_t) &buffer[arg0_start]);
	assert(((uintptr_t *) buffer)[1] == (uintptr_t) &buffer[arg0_start + 5]);
	assert(((uintptr_t *) buffer)[2] == (uintptr_t) &buffer[arg0_start + 10]);

	return 0;
}

