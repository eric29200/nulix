#include <lib/ring_buffer.h>
#include <proc/sched.h>
#include <mm/mm.h>
#include <stderr.h>

/*
 * Init a ring buffer.
 */
int ring_buffer_init(struct ring_buffer_t *rb, size_t capacity)
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
void ring_buffer_destroy(struct ring_buffer_t *rb)
{
	if (rb && rb->buffer)
		kfree(rb->buffer);
}

/*
 * Read from a ring buffer.
 */
size_t ring_buffer_read(struct ring_buffer_t *rb, uint8_t *buf, size_t n)
{
	size_t i;

	for (i = 0; i < n; i++) {
		/* buffer empty : sleep */
		while (rb->size == 0) {
			if (signal_pending(current_task))
				return i;

			task_sleep(&rb->wait);
		}

		/* read from ring buffer */
		buf[i] = rb->buffer[rb->tail++];
		rb->size--;

		/* update position */
		if (rb->tail == rb->capacity)
			rb->tail = 0;

		/* wakeup eventual writers */
		task_wakeup(&rb->wait);
	}

	return n;
}

/*
 * Write to a ring buffer.
 */
size_t ring_buffer_write(struct ring_buffer_t *rb, const uint8_t *buf, size_t n)
{
	size_t i;

	for (i = 0; i < n; i++) {
		/* buffer full : sleep */
		while (rb->size == rb->capacity) {
			if (signal_pending(current_task))
				return i;

			task_sleep(&rb->wait);
		}

		/* write to ring buffer */
		rb->buffer[rb->head++] = buf[i];
		rb->size++;

		/* update position */
		if (rb->head == rb->capacity)
			rb->head = 0;

		/* wakeup eventual readers */
		task_wakeup(&rb->wait);
	}

	return n;
}

/*
 * Put a character in a ring buffer (does not block = if buffer is full, it fails).
 */
int ring_buffer_putc(struct ring_buffer_t *rb, uint8_t c)
{
	if (ring_buffer_full(rb))
		return -EAGAIN;

	/* write to ring buffer */
	rb->buffer[rb->head++] = c;
	rb->size++;

	/* update position */
	if (rb->head == rb->capacity)
		rb->head = 0;

	/* wakeup eventual readers */
	task_wakeup(&rb->wait);

	return 0;
}

/*
 * Flush a ring buffer.
 */
void ring_buffer_flush(struct ring_buffer_t *rb)
{
	/* flush buffer */
	rb->size = 0;
	rb->head = 0;
	rb->tail = 0;

	/* wakeup eventual readers/writers */
	task_wakeup(&rb->wait);
}
