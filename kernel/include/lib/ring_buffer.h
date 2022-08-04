#ifndef _RING_BUFFER_H_
#define _RING_BUFFER_H_

#include <stddef.h>

/*
 * Ring buffer structure.
 */
struct ring_buffer_t {
	size_t		head;
	size_t		tail;
	size_t		capacity;
	size_t		size;
	uint8_t *	buffer;
};

int ring_buffer_init(struct ring_buffer_t *rb, size_t capacity);
void ring_buffer_destroy(struct ring_buffer_t *rb);
size_t ring_buffer_read(struct ring_buffer_t *rb, uint8_t *buf, size_t n);
size_t ring_buffer_write(struct ring_buffer_t *rb, const uint8_t *buf, size_t n);
int ring_buffer_putc(struct ring_buffer_t *rb, uint8_t c);

/*
 * Test if a ring buffer is full.
 */
static inline int ring_buffer_full(struct ring_buffer_t *rb)
{
	return rb->size >= rb->capacity;
}

#endif
