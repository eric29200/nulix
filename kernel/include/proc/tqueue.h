#ifndef _TQUEUE_H_
#define _TQUEUE_H_

#include <lib/list.h>
#include <stddef.h>

/*
 * Task queue.
 */
struct tqueue {
	uint32_t		sync;
	void			(*routine)(void *);
	void *			data;
	struct list_head	list;
};

int queue_task(struct tqueue *tqueue);
void run_task_queues();

/*
 * Init a task queue.
 */
static inline void INIT_TQUEUE(struct tqueue *tqueue, void (*routine)(void *), void *data)
{
	tqueue->sync = 0;
	tqueue->routine = routine;
	tqueue->data = data;
}

#endif