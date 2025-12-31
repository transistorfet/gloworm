
#include <kernel/utils/iovec.h>

// TODO move these to the C file, instead of inlining
int memcpy_into_iter(struct iovec_iter *iter, const void *buf, size_t nbytes)
{
	int bytes;

	bytes = (nbytes < iter->length - iter->offset) ? nbytes : iter->length - iter->offset;
	switch (iter->type) {
		case IOVEC_USER_BUF: {
			memcpy_to_user(&iter->user_buf.buf[iter->offset], buf, bytes);
			break;
		}
		case IOVEC_KERNEL_BUF: {
			memcpy(&iter->kernel_buf.buf[iter->offset], buf, bytes);
			break;
		}
		default: {
			log_error("attempting to write into an invalid iovec iter: %d\n", iter->type);
			return EINVAL;
		}
	}

	iter->offset += bytes;
	return bytes;
}

int memcpy_out_of_iter(struct iovec_iter *iter, void *buf, size_t nbytes)
{
	int bytes;

	bytes = (nbytes < iter->length - iter->offset) ? nbytes : iter->length - iter->offset;
	switch (iter->type) {
		case IOVEC_USER_BUF: {
			memcpy_from_user(buf, &iter->user_buf.buf[iter->offset], bytes);
			break;
		}
		case IOVEC_KERNEL_BUF: {
			memcpy(buf, &iter->kernel_buf.buf[iter->offset], bytes);
			break;
		}
		default: {
			log_error("attempting to write into an invalid iovec iter: %d\n", iter->type);
			return EINVAL;
		}
	}

	iter->offset += bytes;
	return bytes;
}

