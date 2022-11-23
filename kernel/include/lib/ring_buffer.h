#ifndef _RING_BUFFER_H_
#define _RING_BUFFER_H_

#include <proc/wait.h>
#include <stddef.h>

/*
 * Ring buffer structure.
 */
struct ring_buffer_t {
	size_t			head;
	size_t			tail;
	size_t			capacity;
	size_t			size;
	uint8_t *		buffer;
	struct wait_queue_t *	wait;
};

int ring_buffer_init(struct ring_buffer_t *rb, size_t capacity);
void ring_buffer_destroy(struct ring_buffer_t *rb);
size_t ring_buffer_read(struct ring_buffer_t *rb, uint8_t *buf, size_t n);
size_t ring_buffer_write(struct ring_buffer_t *rb, const uint8_t *buf, size_t n);
int ring_buffer_putc(struct ring_buffer_t *rb, uint8_t c);
void ring_buffer_flush(struct ring_buffer_t *rb);

/*
 * Test if a ring buffer is full.
 */
static inline int ring_buffer_full(struct ring_buffer_t *rb)
{
	return rb->size >= rb->capacity;
}

/*
 * Test if a ring buffer is empty.
 */
static inline int ring_buffer_empty(struct ring_buffer_t *rb)
{
	return rb->size == 0;
}

/*
 * Get left space in ring buffer.
 */
static inline int ring_buffer_left(struct ring_buffer_t *rb)
{
	return rb->capacity - rb->size;
}

#endif
