
#ifndef _MM_IOVEC_H
#define _MM_IOVEC_H

#include <errno.h>
#include <sys/types.h>

#include <kconfig.h>
#include <kernel/printk.h>
#include <kernel/utils/usercopy.h>
#include <asm/addresses.h>

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
	iovec_iter_type_t type;

	size_t length;

	size_t seg_start;
	size_t seg_offset;

	int cur_seg;
	int num_segs;

	union {
		/// A buffer in user memory, with length `nbytes`
		struct {
			char __user *buf;
			size_t nbytes;
		} user_buf;

		/// A sequence of iovec buffers, with total number of segments `nsegs`
		struct {
			struct iovec *segs;
		} iovec;

		/// A sequence of kvec buffers, with total number of segments `nsegs`
		struct {
			struct kvec *segs;
		} kvec;
	};
};


int memcpy_into_kvec(struct kvec *kvec, int num_segs, int seg_offset, const void *src, size_t nbytes);
int memcpy_out_of_kvec(struct kvec *kvec, int num_segs, int seg_offset, void *dest, size_t nbytes);
int strncpy_into_kvec(struct kvec *kvec, int num_segs, int seg_offset, const char *src, size_t max);
int strncpy_out_of_kvec(struct kvec *kvec, int num_segs, int seg_offset, char *dest, size_t max);

int memcpy_into_iter(struct iovec_iter *iter, const void *buf, size_t nbytes);
int memcpy_out_of_iter(struct iovec_iter *iter, void *buf, size_t nbytes);
size_t iovec_iter_seek(struct iovec_iter *iter, offset_t offset, int whence);
size_t iovec_iter_push_back(struct iovec_iter *iter, void *value, int size);


static inline void iovec_iter_init_user_buf(struct iovec_iter *iter, char __user *buf, size_t nbytes)
{
	iter->type = ITER_USER_BUF;
	iter->seg_start = 0;
	iter->seg_offset = 0;
	iter->num_segs = 1;
	iter->cur_seg = 0;
	iter->length = nbytes;
	iter->user_buf.buf = buf;
	iter->user_buf.nbytes = nbytes;
}

static inline void iovec_iter_init_simple_kvec(struct iovec_iter *iter, struct kvec *segs, char *buf, size_t nbytes)
{
	segs->buf = buf;
	segs->bytes = nbytes;

	iter->type = ITER_KVEC;
	iter->seg_start = 0;
	iter->seg_offset = 0;
	iter->num_segs = 1;
	iter->cur_seg = 0;
	iter->kvec.segs = segs;
	iter->length = nbytes;
}

static inline void iovec_iter_init_kvec(struct iovec_iter *iter, struct kvec *segs, size_t num_segs)
{
	size_t length = 0;

	for (int i = 0; i < num_segs; i++) {
		length += segs[i].bytes;
	}
	iter->type = ITER_KVEC;
	iter->seg_start = 0;
	iter->seg_offset = 0;
	iter->length = length;
	iter->kvec.segs = segs;
	iter->num_segs = num_segs;
	iter->cur_seg = 0;
}


static inline size_t iovec_iter_remaining(struct iovec_iter *iter)
{
	return iter->length - iter->seg_start - iter->seg_offset;
}

static inline int copy_uint8_into_iter(struct iovec_iter *iter, uint8_t byte)
{
	switch (iter->type) {
		case ITER_USER_BUF: {
			put_user_uint8(&iter->user_buf.buf[iter->seg_offset++], byte);
			break;
		}
		case ITER_KVEC: {
			if (iter->cur_seg > iter->num_segs) {
				return 0;
			}

			iter->kvec.segs[iter->cur_seg].buf[iter->seg_offset++] = byte;
			if (iter->seg_offset >= iter->kvec.segs[iter->cur_seg].bytes) {
				iter->seg_start += iter->kvec.segs[iter->cur_seg].bytes;
				iter->seg_offset = 0;
				iter->cur_seg += 1;
			}
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
		case ITER_USER_BUF: {
			return get_user_uint8(&iter->user_buf.buf[iter->seg_offset++]);
		}
		case ITER_KVEC: {
			if (iter->cur_seg > iter->num_segs) {
				return 0;
			}

			uint8_t byte = iter->kvec.segs[iter->cur_seg].buf[iter->seg_offset++];
			if (iter->seg_offset >= iter->kvec.segs[iter->cur_seg].bytes) {
				iter->seg_start += iter->kvec.segs[iter->cur_seg].bytes;
				iter->seg_offset = 0;
				iter->cur_seg += 1;
			}
			return byte;
		}
		default: {
			log_error("attempting to write into an invalid iovec iter: %d\n", iter->type);
			return -1;
		}
	}
}

struct memory_map;
extern int memory_map_load_pages_into_kvec(struct memory_map *map, struct kvec *kvec, int max_segs, virtual_address_t start, size_t length, int write_flag);

#if defined(CONFIG_MMU)

static inline int iovec_iter_load_pages_iter(struct memory_map *map, struct iovec_iter *iter, struct kvec *kvec, int max_segs, virtual_address_t start, size_t length, int write_flag)
{
	int result;

	result = memory_map_load_pages_into_kvec(map, kvec, max_segs, start, length, write_flag);
	if (result < 0)
		return result;
	iovec_iter_init_kvec(iter, kvec, result);
	return 0;
}

#else

static inline int iovec_iter_load_pages_iter(struct memory_map *map, struct iovec_iter *iter, struct kvec *kvec, int max_segs, virtual_address_t start, size_t length, int write_flag)
{
	iovec_iter_init_simple_kvec(iter, kvec, (char *) start, length);
	return 0;
}

#endif	// CONFIG_MMU

#endif

