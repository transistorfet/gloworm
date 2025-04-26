
#ifndef _KERNEL_UTILS_RINGBUFFER_H
#define _KERNEL_UTILS_RINGBUFFER_H

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


#endif

