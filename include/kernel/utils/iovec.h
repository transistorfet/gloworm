
#ifndef _MM_IOVEC_H
#define _MM_IOVEC_H

#include <errno.h>
#include <sys/types.h>

#include <kconfig.h>
#include <kernel/printk.h>
#include <kernel/utils/usercopy.h>

#ifndef SEEK_SET
#define SEEK_SET	0	// Seek relative to the beginning of file
#endif
#ifndef SEEK_CUR
#define SEEK_CUR	1	// Seek relative to the current position
#endif
#ifndef SEEK_END
#define SEEK_END	2	// Seek relative to the end of file
#endif

#define IOVEC_READ	0
#define IOVEC_WRITE	1


typedef enum {
	ITER_USER_BUF = 1,
	ITER_KERNEL_BUF = 2,
	ITER_KVEC = 3,
} iovec_iter_type_t;

struct iovec {
	char __user *buf;
	size_t bytes;
};

struct kvec {
	char *buf;
	size_t bytes;
};

struct iovec_iter {
	//iovec_iter_type_t type;
	int type;
	size_t offset;

	// TODO remove?
	size_t length;

	int cur_segs;
	int num_segs;

	union {
		/// A buffer in user memory, with length `nbytes`
		struct {
			char __user *buf;
			size_t nbytes;
		} user_buf;

		/// A buffer in kernel memory, with length `nbytes`
		struct {
			char *buf;
			size_t nbytes;
		} kernel_buf;

		/// A sequence of iovec buffers, with total number of segments `nsegs`
		struct {
			struct iovec *segs;
int nsegs;

		} iovec;


	};
};

struct kvec_iter {
	struct kvec kvec[1];
	struct iovec_iter iter;
};


int memcpy_into_iter(struct iovec_iter *iter, const void *buf, size_t nbytes);
int memcpy_out_of_iter(struct iovec_iter *iter, void *buf, size_t nbytes);


static inline void iovec_iter_init_user_buf(struct iovec_iter *iter, char __user *buf, size_t nbytes)
{
	iter->type = ITER_USER_BUF;
	iter->offset = 0;
	//iter->num_segs = 1;
	//iter->cur_seg = 0;
	iter->length = nbytes;
	iter->user_buf.buf = buf;
	iter->user_buf.nbytes = nbytes;
}

static inline void iovec_iter_init_kernel_buf(struct iovec_iter *iter, char *buf, size_t nbytes)
{
	iter->type = ITER_KERNEL_BUF;
	iter->offset = 0;
	//iter->num_segs = 1;
	//iter->cur_seg = 0;
	iter->length = nbytes;
	iter->kernel_buf.buf = buf;
	iter->kernel_buf.nbytes = nbytes;
}

/*
static inline void iovec_iter_init_kvec(struct iovec_iter *iter, struct kvec *segs, size_t num_segs)
{

	iter->type = ITER_KERNEL_BUF;
	iter->offset = 0;
	iter->length = -1;
	iter->kvec.segs = segs;
	iter->num_segs = num_segs;
	iter->cur_seg = 0;
}
*/


static inline size_t iovec_iter_length(struct iovec_iter *iter)
{
//printk("length of %x is %d offset %d %d\n", iter, iter->length, iter->offset, sizeof(struct iovec_iter));
	return iter->length - iter->offset;
/*
	size_t length = 0;

	switch (iter->type) {
		case ITER_USER_BUF:
		case ITER_KERNEL_BUF:
			return iter->length - iter->offset;
		case ITER_KVEC:
			for (int i = 0; i < iter->num_segs; i++) {
				length += iter->kvec.segs[iter->num_segs].bytes;
			}
			return length;
		default:
			return 0;
	}
*/
}

/// Change the read/write position of an iterator
static inline size_t iovec_iter_seek(struct iovec_iter *iter, offset_t offset, int whence)
{
	switch (iter->type) {
		case ITER_USER_BUF:
		case ITER_KERNEL_BUF:
			switch (whence) {
				case SEEK_SET:
					break;
				case SEEK_CUR:
					offset = iter->offset + offset;
					break;
				case SEEK_END:
					offset = iter->length + offset;
					break;
				default:
					return EINVAL;
			}

break;
/*
			if (offset > iter->length)
				offset = iter->length;

			iter->offset = offset;
			return iter->offset;
*/
/*
		case ITER_KVEC:
			switch (whence) {
				case SEEK_SET:
					iter->cur_seg = 0;
					iter->offset = 0;
					break;
				case SEEK_CUR:
					//offset = iter->offset + offset;
					// Use the current cursor position
					break;
				case SEEK_END:
					//offset = iter->length + offset;
					iter->cur_seg = iter->num_segs - 1;
					iter->offset = iter->kvec.segs[iter->cur_seg].bytes - 1;
					break;
				default:
					return EINVAL;
			}

			while (iter->cur_seg >= 0 && iter->cur_seg < iter->num_segs) {
				if (offset < 0) {
					if (offset > iter->offset) {
						offset -= iter->offset;
						iter->offset = 0;
						if (iter->cur_seg == 0) {
							return 0;
						}
						iter->cur_seg -= 1;
					} else {
						iter->offset += offset;
						break;
					}
				} else {
					if (iter->offset + offset >= iter->kvec.segs[iter->cur_seg].bytes) {
						offset -= iter->kvec.segs[iter->cur_seg].bytes;
						iter->cur_seg += 1;
						iter->offset = 0;
					} else {
						iter->offset += offset;
						break;
					}
				}
			}

			return iter->offset;
*/
		default: {
			log_error("attempting to seek into an invalid iovec iter: %d\n", iter->type);
			return EINVAL;
		}
	}

	if (offset > iter->length)
		offset = iter->length;

	iter->offset = offset;
	return iter->offset;
}

static inline int copy_uint8_into_iter(struct iovec_iter *iter, uint8_t byte)
{
	switch (iter->type) {
		case ITER_USER_BUF: {
			put_user_uint8(&iter->user_buf.buf[iter->offset++], byte);
			//memcpy_to_user(&iter->user_buf.buf[iter->offset++], &byte, 1);
			break;
		}
		case ITER_KERNEL_BUF: {
			iter->kernel_buf.buf[iter->offset++] = byte;
			break;
		}
/*
		case ITER_KVEC: {
			if (iter->cur_seg > iter->num_segs) {
				return 0;
			}

			iter->kvec.segs[iter->cur_seg].buf[iter->offset++] = byte;
			if (iter->offset >= iter->kvec.segs[iter->cur_seg].bytes) {
				iter->offset = 0;
				iter->cur_seg += 1;
			}
			break;
		}
*/
		default: {
			log_error("attempting to write into an invalid iovec iter: %d\n", iter->type);
			return EINVAL;
		}
	}
	return 1;
}

static inline uint8_t copy_uint8_out_of_iter(struct iovec_iter *iter)
{
	switch (iter->type) {
		case ITER_USER_BUF: {
			return get_user_uint8(&iter->user_buf.buf[iter->offset++]);
			//char ch;
			//memcpy_from_user(&ch, &iter->user_buf.buf[iter->offset++], 1);
			//return ch;
		}
		case ITER_KERNEL_BUF: {
			return iter->kernel_buf.buf[iter->offset++];
		}
/*
		case ITER_KVEC: {
			if (iter->cur_seg > iter->num_segs) {
				return 0;
			}

			uint8_t byte = iter->kvec.segs[iter->cur_seg].buf[iter->offset++];
			if (iter->offset >= iter->kvec.segs[iter->cur_seg].bytes) {
				iter->offset = 0;
				iter->cur_seg += 1;
			}
			return byte;
		}
*/
		default: {
			log_error("attempting to write into an invalid iovec iter: %d\n", iter->type);
			return -1;
		}
	}
}

static inline size_t iovec_iter_push_back(struct iovec_iter *iter, void *value, int size)
{
	offset_t position;

	if (iter->offset < size) {
		return -1;
	}

	position = iter->offset;
	position -= size;
	iter->offset = position;
	memcpy_into_iter(iter, value, size);
	iter->offset = position;
	return iter->offset;
}

#endif

