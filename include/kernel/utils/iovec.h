
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

typedef enum {
	IOVEC_USER_BUF = 1,
	IOVEC_KERNEL_BUF = 2,
	IOVEC_ARRAY = 3,
} iovec_type_t;

struct iovec {
	const char __user *buf;
	int bytes;
};

struct iovec_iter {
	iovec_type_t type;
	size_t offset;
	size_t length;

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


int memcpy_into_iter(struct iovec_iter *iter, const void *buf, size_t nbytes);
int memcpy_out_of_iter(struct iovec_iter *iter, void *buf, size_t nbytes);


static inline void iovec_iter_init_user_buf(struct iovec_iter *iter, char __user *buf, size_t nbytes)
{
	iter->type = IOVEC_USER_BUF;
	iter->offset = 0;
	iter->length = nbytes;
	iter->user_buf.buf = buf;
	iter->user_buf.nbytes = nbytes;
}

static inline void iovec_iter_init_kernel_buf(struct iovec_iter *iter, char *buf, size_t nbytes)
{
	iter->type = IOVEC_KERNEL_BUF;
	iter->offset = 0;
	iter->length = nbytes;
	iter->kernel_buf.buf = buf;
	iter->kernel_buf.nbytes = nbytes;
}

static inline size_t iovec_iter_length(struct iovec_iter *iter)
{
	return iter->length - iter->offset;
}

/// Change the read/write position of an iterator
static inline size_t iovec_iter_seek(struct iovec_iter *iter, offset_t offset, int whence)
{
	switch (iter->type) {
		case IOVEC_USER_BUF:
		case IOVEC_KERNEL_BUF:
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
		case IOVEC_USER_BUF: {
			put_user_uint8(&iter->user_buf.buf[iter->offset++], byte);
			break;
		}
		case IOVEC_KERNEL_BUF: {
			iter->kernel_buf.buf[iter->offset++] = byte;
			break;
		}
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
		case IOVEC_USER_BUF: {
			return get_user_uint8(&iter->user_buf.buf[iter->offset++]);
		}
		case IOVEC_KERNEL_BUF: {
			return iter->kernel_buf.buf[iter->offset++];
		}
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

