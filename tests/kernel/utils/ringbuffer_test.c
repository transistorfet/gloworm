
#include <assert.h>
#include <stdio.h>
#include <kernel/utils/iovec.h>
#include <kernel/utils/ringbuffer.h>

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
	printf("Used?: %d\n", _buf_used_space(&buffer));
	printf("Available?: %d\n", _buf_free_space(&buffer));
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

	struct iovec_iter iter;
	char array[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };

	iovec_iter_init_kernel_buf(&iter, array, 10);
	_buf_put_iter(&buffer, &iter);
	printf("Used: %d\n", _buf_used_space(&buffer));
	//assert(_buf_used_space(&buffer) == 10);

	struct iovec_iter iter2;
	char array2[10];

	iovec_iter_init_kernel_buf(&iter2, array2, 10);
	_buf_get_iter(&buffer, &iter);
	printf("%d\n", _buf_used_space(&buffer));
	//assert(_buf_used_space(&buffer) == 0);
	for (int i = 0; i < 10; i++) {
		printf("%d ", array2[i]);
		//assert(array[i] == array2[i]);
	}
	printf("\n");

	return 0;
}

