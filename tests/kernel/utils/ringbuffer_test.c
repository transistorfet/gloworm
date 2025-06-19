
#include <assert.h>
#include <stdio.h>
#include "../include/kernel/utils/ringbuffer.h"

#define BUF_SIZE	16

int main()
{
	struct ringbuffer buffer;

	_buf_init(&buffer, BUF_SIZE);

	printf("Empty?: %d\n", _buf_is_empty(&buffer));
	printf("Full?: %d\n", _buf_is_full(&buffer));
	assert(_buf_is_empty(&buffer));

	for (int i = 0; i < BUF_SIZE + 1; i++) {
		int res = _buf_put_char(&buffer, '0' + (i % 10));
		if (res == -1)
			printf("Full\n");
	}

	printf("Empty?: %d\n", _buf_is_empty(&buffer));
	printf("Full?: %d\n", _buf_is_full(&buffer));
	assert(_buf_is_full(&buffer));

	for (int i = 0; i < BUF_SIZE; i++) {
		int res = _buf_get_char(&buffer);
		if (res == -1) {
			printf("Empty\n");
		} else {
			//printf("Read %c\n", res);
		}
	}

	printf("Empty?: %d\n", _buf_is_empty(&buffer));
	printf("Full?: %d\n", _buf_is_full(&buffer));

	for (int i = 0; i < 4; i++) {
		int res = _buf_put_char(&buffer, '0' + i);
		if (res == -1)
			printf("Full\n");
	}

	printf("Empty?: %d\n", _buf_is_empty(&buffer));
	printf("Full?: %d\n", _buf_is_full(&buffer));

	for (int i = 0; i < 20; i++) {
		int res = _buf_get_char(&buffer);
		if (res == -1) {
			printf("Empty\n");
		} else {
			printf("Read %c\n", res);
		}
	}

	printf("In: %d\n", buffer.in);
	printf("Out: %d\n", buffer.out);

	return 0;
}

