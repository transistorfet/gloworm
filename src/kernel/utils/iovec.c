
#include <kernel/utils/iovec.h>

/*
static inline int kvec_memcpy_into_iter(struct iovec_iter *iter, const void *src, size_t nbytes)
{
	char *dest_buf;
	size_t buf_size;
	size_t chunk_size;
	size_t src_index = 0;

	while (iter->cur_seg < iter->num_segs) {
		dest_buf = (char *) iter->kvec.segs[iter->cur_seg].buf;
		buf_size = iter->kvec.segs[iter->cur_seg].bytes;

		chunk_size = (nbytes - src_index < buf_size - iter->offset) ? nbytes - src_index : buf_size - iter->offset;
		memcpy(&dest_buf[iter->offset], &((char *) src)[src_index], chunk_size);
		iter->offset += chunk_size;
		if (iter->offset >= buf_size) {
			iter->offset = 0;
			iter->cur_seg += 1;
		}

		src_index += chunk_size;
		if (src_index >= nbytes) {
			return src_index;
		}
	}
	return src_index;
}

static inline int kvec_memcpy_out_of_iter(struct iovec_iter *iter, const void *dest, size_t nbytes)
{
	char *src_buf;
	size_t buf_size;
	size_t chunk_size;
	size_t dest_index = 0;

	while (iter->cur_seg < iter->num_segs) {
		src_buf = (char *) iter->kvec.segs[iter->cur_seg].buf;
		buf_size = iter->kvec.segs[iter->cur_seg].bytes;

		chunk_size = (nbytes - dest_index < buf_size - iter->offset) ? nbytes - dest_index : buf_size - iter->offset;
		memcpy(&((char *) dest)[dest_index], &src_buf[iter->offset], chunk_size);
		iter->offset += chunk_size;
		if (iter->offset >= buf_size) {
			iter->offset = 0;
			iter->cur_seg += 1;
		}

		dest_index += chunk_size;
		if (dest_index >= nbytes) {
			return dest_index;
		}
	}
	return dest_index;
}
*/

// TODO move these to the C file, instead of inlining
int memcpy_into_iter(struct iovec_iter *iter, const void *buf, size_t nbytes)
{
	int bytes;

	bytes = (nbytes < iter->length - iter->offset) ? nbytes : iter->length - iter->offset;
	switch (iter->type) {
		case ITER_USER_BUF: {
			memcpy_to_user(&iter->user_buf.buf[iter->offset], buf, bytes);
			break;
		}
		case ITER_KERNEL_BUF: {
			memcpy(&iter->kernel_buf.buf[iter->offset], buf, bytes);
			break;
		}
/*
		case ITER_KVEC: {
			kvec_memcpy_into_iter(iter, buf, bytes);
			break;
		}
*/
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
		case ITER_USER_BUF: {
			memcpy_from_user(buf, &iter->user_buf.buf[iter->offset], bytes);
			break;
		}
		case ITER_KERNEL_BUF: {
			memcpy(buf, &iter->kernel_buf.buf[iter->offset], bytes);
			break;
		}
/*
		case ITER_KVEC: {
			kvec_memcpy_out_of_iter(iter, buf, bytes);
			break;
		}
*/
		default: {
			log_error("attempting to write into an invalid iovec iter: %d\n", iter->type);
			return EINVAL;
		}
	}

	iter->offset += bytes;
	return bytes;
}

