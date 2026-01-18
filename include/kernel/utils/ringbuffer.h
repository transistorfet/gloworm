
#ifndef _KERNEL_UTILS_RINGBUFFER_H
#define _KERNEL_UTILS_RINGBUFFER_H

#include <kernel/utils/iovec.h>

#define RING_BUFFER_SIZE	512

struct ringbuffer {
	short max;
	volatile short in;
	volatile short out;
	volatile unsigned char buffer[RING_BUFFER_SIZE];
};


static inline void _buf_init(struct ringbuffer *cb, short total_size)
{
	cb->max = total_size ? total_size - sizeof(short) * 3 : RING_BUFFER_SIZE;
	cb->in = 0;
	cb->out = 0;
}

static inline short _buf_next_in(struct ringbuffer *cb)
{
	return cb->in + 1 < cb->max ? cb->in + 1 : 0;
}

static inline int _buf_is_full(struct ringbuffer *cb)
{
	return _buf_next_in(cb) == cb->out;
}

static inline int _buf_is_empty(struct ringbuffer *cb)
{
	return cb->in == cb->out;
}

static inline short _buf_used_space(struct ringbuffer *cb)
{
	if (cb->in >= cb->out) {
		return cb->in - cb->out;
	} else {
		return cb->max - cb->out + cb->in;
	}
}

static inline short _buf_free_space(struct ringbuffer *cb)
{
	if (cb->out > cb->in) {
		return cb->out - cb->in - 1;
	} else {
		return cb->max - cb->in + cb->out - 1;
	}
}

static inline int _buf_get_char(struct ringbuffer *cb)
{
	register unsigned char ch;

	if (cb->out == cb->in)
		return -1;
	ch = cb->buffer[cb->out++];
	if (cb->out >= cb->max)
		cb->out = 0;
	return ch;
}

static inline int _buf_put_char(struct ringbuffer *cb, unsigned char ch)
{
	register short next;

	next = _buf_next_in(cb);
	if (next == cb->out)
		return -1;
	cb->buffer[cb->in] = ch;
	cb->in = next;
	return ch;
}


static inline short _buf_get(struct ringbuffer *cb, unsigned char *data, short size)
{
	short i;

	for (i = 0; i < size; i++) {
		if (cb->out == cb->in)
			return i;
		data[i] = cb->buffer[cb->out++];
		if (cb->out >= cb->max)
			cb->out = 0;
	}
	return i;
}

static inline short _buf_peek(struct ringbuffer *cb, unsigned char *data, short offset, short size)
{
	short i, j;
	short avail;

	avail = _buf_used_space(cb);
	if (offset > avail)
		offset = avail;
	j = cb->out + offset;
	if (j > cb->max)
		j -= cb->max;

	for (i = 0; i < size; i++) {
		if (j == cb->in)
			return i;
		data[i] = cb->buffer[j++];
		if (j >= cb->max)
			j = 0;
	}
	return i;
}

static inline short _buf_put(struct ringbuffer *cb, const unsigned char *data, short size)
{
	short i;
	register short next;

	for (i = 0; i < size; i++) {
		cb->buffer[cb->in] = data[i];
		next = _buf_next_in(cb);
		if (next == cb->out)
			return i;
		cb->in = next;
	}
	return i;
}

static inline short _buf_drop(struct ringbuffer *cb, short size)
{
	short avail = _buf_used_space(cb);

	if (size > avail)
		size = avail;

	cb->out += size;
	if (cb->out >= cb->max)
		cb->out -= cb->max;
	return size;
}

static inline short _buf_get_iter(struct ringbuffer *cb, struct iovec_iter *iter)
{
	size_t size;
	short chunk1, chunk2;

	size = iovec_iter_length(iter);

	// Calculate the maximum readable chunk from the current out pointer to either
	// the in pointer, or if the in pointer is less than the out pointer, then to
	// the end of the buffer
	if (cb->in > cb->out) {
		chunk1 = (cb->in - cb->out < size) ? cb->in - cb->out : size;
	} else {
		chunk1 = (cb->max - cb->out < size) ? cb->max - cb->out : size;
	}

	memcpy_into_iter(iter, (char *) &cb->buffer[cb->out], chunk1);

	cb->out += chunk1;
	if (cb->out >= cb->max)
		cb->out = 0;

	// If we've read the full size, or the buffer is empty, then return early
	size -= chunk1;
	if (size <= 0 || cb->out == cb->in)
		return chunk1;

	// Calculate the maximum readable second chunk, from the bottom of the buffer to
	// the in pointer, or the remaining size if there's more data available than requested
	chunk2 = (cb->in - cb->out < size) ? cb->in - cb->out : size;

	memcpy_into_iter(iter, (char *) &cb->buffer[cb->out], chunk2);
	cb->out += chunk2;

	return chunk1 + chunk2;
}

static inline short _buf_put_iter(struct ringbuffer *cb, struct iovec_iter *iter)
{
	size_t size;
	short chunk1, chunk2;

	size = iovec_iter_length(iter);

	// Calculate the maximum writable chunk from the current in pointer to either
	// the out pointer, or if the out pointer is less than the in pointer, then to
	// the end of the buffer
	if (cb->out > cb->in) {
		chunk1 = (cb->out - cb->in - 1 < size) ? cb->out - cb->in - 1 : size;
	} else {
		chunk1 = (cb->max - cb->in < size) ? cb->max - cb->in - (cb->out ? 0 : 1) : size;
	}

	memcpy_out_of_iter(iter, (char *) &cb->buffer[cb->in], chunk1);

	cb->in += chunk1;
	if (cb->in >= cb->max) {
		cb->in = 0;
	}

	// If we've read the full size, or the buffer is empty, then return early
	size -= chunk1;
	if (size <= 0 || cb->in + 1 == cb->out || cb->in + 1 == cb->max) {
		return chunk1;
	}

	// Calculate the maximum writable second chunk, from the bottom of the buffer to
	// the out pointer, or the remaining size if there's more data available than requested
	chunk2 = (cb->out - cb->in - 1 < size) ? cb->out - cb->in - 1 : size;

	memcpy_out_of_iter(iter, (char *) &cb->buffer[cb->in], chunk2);
	cb->in += chunk2;

	return chunk1 + chunk2;
}

#endif

