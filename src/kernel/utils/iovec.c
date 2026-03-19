
#include <kernel/utils/iovec.h>

int kvec_memcpy_into_iter(struct kvec *kvec, int num_segs, int seg_offset, const void *src, size_t nbytes)
{
	int cur_seg = 0;
	char *dest_buf;
	size_t buf_size;
	size_t chunk_size;
	size_t src_index = 0;

	while (1) {
		dest_buf = (char *) kvec[cur_seg].buf;
		buf_size = kvec[cur_seg].bytes;

		if (seg_offset >= buf_size) {
			if (cur_seg + 1 >= num_segs) {
				break;
			}
			cur_seg += 1;
			seg_offset = 0;
		}

		chunk_size = (nbytes - src_index < buf_size - seg_offset) ? nbytes - src_index : buf_size - seg_offset;
		memcpy(&dest_buf[seg_offset], &((char *) src)[src_index], chunk_size);
		seg_offset += chunk_size;

		src_index += chunk_size;
		if (src_index >= nbytes) {
			return src_index;
		}
	}
	return src_index;
}

int kvec_memcpy_out_of_iter(struct kvec *kvec, int num_segs, int seg_offset, void *dest, size_t nbytes)
{
	int cur_seg = 0;
	char *src_buf;
	size_t buf_size;
	size_t chunk_size;
	size_t dest_index = 0;

	while (1) {
		src_buf = (char *) kvec[cur_seg].buf;
		buf_size = kvec[cur_seg].bytes;

		if (seg_offset >= buf_size) {
			if (cur_seg + 1 >= num_segs) {
				break;
			}
			seg_offset = 0;
			cur_seg += 1;
		}

		chunk_size = (nbytes - dest_index < buf_size - seg_offset) ? nbytes - dest_index : buf_size - seg_offset;
		memcpy(&((char *) dest)[dest_index], &src_buf[seg_offset], chunk_size);
		seg_offset += chunk_size;

		dest_index += chunk_size;
		if (dest_index >= nbytes) {
			return dest_index;
		}
	}
	return dest_index;
}

int kvec_strncpy_into_iter(struct kvec *kvec, int num_segs, int seg_offset, const char *src, size_t max)
{
	int cur_seg = 0;
	char *dest_buf;
	size_t buf_size;
	size_t src_index = 0;

	while (cur_seg < num_segs) {
		dest_buf = (char *) kvec[cur_seg].buf;
		buf_size = kvec[cur_seg].bytes;

		for (; 1; seg_offset++, src_index++) {
			if (seg_offset >= buf_size) {
				seg_offset = 0;
				cur_seg += 1;
				break;
			}

			if (src_index >= max) {
				return src_index;
			}

			dest_buf[seg_offset] = src[src_index];

			if (src[src_index] == '\0') {
				return src_index;
			}
		}
	}
	return src_index;
}

int kvec_strncpy_out_of_iter(struct kvec *kvec, int num_segs, int seg_offset, char *dest, size_t max)
{
	int cur_seg = 0;
	char *src_buf;
	size_t buf_size;
	size_t dest_index = 0;

	while (cur_seg < num_segs) {
		src_buf = (char *) kvec[cur_seg].buf;
		buf_size = kvec[cur_seg].bytes;

		for (; 1; seg_offset++, dest_index++) {
			if (seg_offset >= buf_size) {
				seg_offset = 0;
				cur_seg += 1;
				break;
			}

			if (dest_index >= max) {
				return dest_index;
			}

			dest[dest_index] = src_buf[seg_offset];

			if (dest[dest_index] == '\0') {
				return dest_index;
			}
		}
	}
	return dest_index;
}

int memcpy_into_iter(struct iovec_iter *iter, const void *buf, size_t nbytes)
{
	int bytes;

	bytes = (nbytes < iter->length - iter->seg_start - iter->seg_offset) ? nbytes : iter->length - iter->seg_start - iter->seg_offset;
	switch (iter->type) {
		case ITER_USER_BUF: {
			memcpy_to_user(&iter->user_buf.buf[iter->seg_offset], buf, bytes);
			iter->seg_offset += bytes;
			return bytes;
		}
		case ITER_KERNEL_BUF: {
			memcpy(&iter->kernel_buf.buf[iter->seg_offset], buf, bytes);
			iter->seg_offset += bytes;
			return bytes;
		}
		case ITER_KVEC: {
			bytes = kvec_memcpy_into_iter(&iter->kvec.segs[iter->cur_seg], iter->num_segs - iter->cur_seg, iter->seg_offset, buf, bytes);
			if (bytes > 0) {
				iovec_iter_seek(iter, bytes, SEEK_CUR);
			}
			return bytes;
		}
		default: {
			log_error("attempting to write into an invalid iovec iter: %d\n", iter->type);
			return EINVAL;
		}
	}
}

int memcpy_out_of_iter(struct iovec_iter *iter, void *buf, size_t nbytes)
{
	int bytes;

	bytes = (nbytes < iter->length - iter->seg_start - iter->seg_offset) ? nbytes : iter->length - iter->seg_start - iter->seg_offset;
	switch (iter->type) {
		case ITER_USER_BUF: {
			memcpy_from_user(buf, &iter->user_buf.buf[iter->seg_offset], bytes);
			iter->seg_offset += bytes;
			return bytes;
		}
		case ITER_KERNEL_BUF: {
			memcpy(buf, &iter->kernel_buf.buf[iter->seg_offset], bytes);
			iter->seg_offset += bytes;
			return bytes;
		}
		case ITER_KVEC: {
			bytes = kvec_memcpy_out_of_iter(&iter->kvec.segs[iter->cur_seg], iter->num_segs - iter->cur_seg, iter->seg_offset, buf, bytes);
			if (bytes > 0) {
				iovec_iter_seek(iter, bytes, SEEK_CUR);
			}
			return bytes;
		}
		default: {
			log_error("attempting to write into an invalid iovec iter: %d\n", iter->type);
			return EINVAL;
		}
	}
}

