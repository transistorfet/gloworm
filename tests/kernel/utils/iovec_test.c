
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <kernel/utils/iovec.h>

#define KVEC_SIZE	10

void assert_iter_position(struct iovec_iter *iter, int result, int expected_result, int expected_cur_seg, int expected_seg_offset)
{
	printk("result: %d\n", result);
	assert(result == expected_result);
	printk("cur_seg: %d\n", iter->cur_seg);
	assert(iter->cur_seg == expected_cur_seg);
	printk("seg_offset: %d\n", iter->seg_offset);
	assert(iter->seg_offset == expected_seg_offset);
	printk("\n");
}

int main(void)
{
	int result;
	int num_args = 3;
	struct iovec_iter iter;
	struct kvec kvec[KVEC_SIZE];
	char page1[256], page2[256];

	kvec[0].buf = page1;
	kvec[0].bytes = 256;
	kvec[1].buf = page2;
	kvec[1].bytes = 256;
	iovec_iter_init_kvec(&iter, kvec, 2);
	printk("length: %d\n", iter.length);
	assert(iter.length == 512);

	result = iovec_iter_seek(&iter, 10, SEEK_CUR);
	assert_iter_position(&iter, result, 10, 0, 10);

	result = iovec_iter_seek(&iter, 10, SEEK_CUR);
	assert_iter_position(&iter, result, 20, 0, 20);

	result = iovec_iter_seek(&iter, 0, SEEK_END);
	assert_iter_position(&iter, result, 512, 1, 256);

	result = iovec_iter_push_back(&iter, "test", 4);
	assert_iter_position(&iter, result, 508, 1, 252);

	result = iovec_iter_push_back(&iter, "what", 4);
	assert_iter_position(&iter, result, 504, 1, 248);

	result = iovec_iter_seek(&iter, -248, SEEK_CUR);
	assert_iter_position(&iter, result, 256, 1, 0);

	result = iovec_iter_push_back(&iter, "test", 4);
	assert_iter_position(&iter, result, 252, 0, 252);

	result = iovec_iter_seek(&iter, 488, SEEK_SET);
	assert_iter_position(&iter, result, 488, 1, 232);

	result = iovec_iter_seek(&iter, 0, SEEK_SET);
	assert_iter_position(&iter, result, 0, 0, 0);

	result = iovec_iter_seek(&iter, 488, SEEK_SET);
	assert_iter_position(&iter, result, 488, 1, 232);

	result = iovec_iter_seek(&iter, 508, SEEK_SET);
	assert_iter_position(&iter, result, 508, 1, 252);

	result = memcpy_into_iter(&iter, "0123", 4);
	assert_iter_position(&iter, result, 4, 1, 256);

	result = iovec_iter_seek(&iter, 488, SEEK_SET);
	assert_iter_position(&iter, result, 488, 1, 232);

	result = memcpy_into_iter(&iter, "01234567890123456789", 20);
	assert_iter_position(&iter, result, 20, 1, 252);

	return 0;
}

