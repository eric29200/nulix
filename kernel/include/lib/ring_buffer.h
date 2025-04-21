#ifndef _RING_BUFFER_H_
#define _RING_BUFFER_H_

#include <proc/sched.h>
#include <stdio.h>
#include <stderr.h>
#include <stddef.h>

/*
 * Ring buffer structure.
 */
struct ring_buffer {
	size_t			head;
	size_t			tail;
	size_t			capacity;
	size_t			size;
	uint8_t *		buffer;
	struct wait_queue *	wait;
};

/*
 * Test if a ring buffer is full.
 */
static inline int ring_buffer_full(struct ring_buffer *rb)
{
	return rb->size >= rb->capacity;
}

/*
 * Test if a ring buffer is empty.
 */
static inline int ring_buffer_empty(struct ring_buffer *rb)
{
	return rb->size == 0;
}

/*
 * Get left space in ring buffer.
 */
static inline int ring_buffer_left(struct ring_buffer *rb)
{
	return rb->capacity - rb->size;
}

/*
 * Init a ring buffer.
 */
static inline int ring_buffer_init(struct ring_buffer *rb, size_t capacity)
{
	/* check capacity */
	if (!rb || capacity <= 0)
		return -EINVAL;

	/* allocate buffer */
	rb->buffer = (uint8_t *) kmalloc(capacity);
	if (!rb->buffer)
		return -ENOMEM;

	/* set ring buffer */
	rb->capacity = capacity;
	rb->size = 0;
	rb->head = 0;
	rb->tail = 0;
	rb->wait = NULL;

	return 0;
}

/*
 * Destroy a ring buffer.
 */
static inline void ring_buffer_destroy(struct ring_buffer *rb)
{
	if (rb && rb->buffer)
		kfree(rb->buffer);
}

/*
 * Get next character from a ring buffer.
 */
static inline int ring_buffer_getc(struct ring_buffer *rb, uint8_t *c, int nonblock)
{
	/* buffer empty */
	while (rb->size == 0) {
		/* non blocking : return */
		if (nonblock)
			return -EAGAIN;

		/* signal received */
		if (signal_pending(current_task))
			return -EINTR;

		/* wait for data */
		sleep_on(&rb->wait);
	}

	/* read from ring buffer */
	*c = rb->buffer[rb->tail++];
	rb->size--;

	/* update position */
	if (rb->tail == rb->capacity)
		rb->tail = 0;

	return 0;
}

/*
 * Put a character in a ring buffer (does not block = if buffer is full, it fails).
 */
static inline int ring_buffer_putc(struct ring_buffer *rb, uint8_t c)
{
	if (ring_buffer_full(rb))
		return -EAGAIN;

	/* write to ring buffer */
	rb->buffer[rb->head++] = c;
	rb->size++;

	/* update position */
	if (rb->head == rb->capacity)
		rb->head = 0;

	/* wake up eventual readers */
	wake_up(&rb->wait);

	return 0;
}

/*
 * Write to a ring buffer.
 */
static inline size_t ring_buffer_write(struct ring_buffer *rb, const uint8_t *buf, size_t n, int nonblock)
{
	size_t i;

	for (i = 0; i < n; i++) {
		/* buffer full : sleep */
		while (rb->size == rb->capacity) {
			/* return data */
			if (i)
				return i;

			/* non blocking : return */
			if (nonblock)
				return -EAGAIN;

			/* signal received */
			if (signal_pending(current_task))
				return -EINTR;

			/* wait for data */
			sleep_on(&rb->wait);
		}

		/* write to ring buffer */
		rb->buffer[rb->head++] = buf[i];
		rb->size++;

		/* update position */
		if (rb->head == rb->capacity)
			rb->head = 0;

		/* wake up eventual readers */
		wake_up(&rb->wait);
	}

	return i;
}

/*
 * Flush a ring buffer.
 */
static inline void ring_buffer_flush(struct ring_buffer *rb)
{
	/* flush buffer */
	rb->size = 0;
	rb->head = 0;
	rb->tail = 0;

	/* wake up eventual readers/writers */
	wake_up(&rb->wait);
}

#endif