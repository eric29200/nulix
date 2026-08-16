#include <proc/tqueue.h>
#include <x86/bitops.h>

/* task queues */
LIST_HEAD(tqueues);

/*
 * Queue a task.
 */
int queue_task(struct tqueue *tqueue)
{
	if (test_and_set_bit(&tqueue->sync, 0))
		return 0;

	list_add_tail(&tqueue->list, &tqueues);
	return 1;
}

/*
 * Unqueue a task.
 */
int unqueue_task(struct tqueue *tqueue)
{
	if (!test_and_clear_bit(&tqueue->sync, 0))
		return 0;

	list_del(&tqueue->list);
	return 1;
}

/*
 * Run task queues.
 */
void run_task_queues()
{
	struct list_head *pos, *n;
	struct tqueue *tqueue;

	list_for_each_safe(pos, n, &tqueues) {
		tqueue = list_entry(pos, struct tqueue, list);
		tqueue->sync = 0;
		list_del(&tqueue->list);
		if (tqueue->routine)
			tqueue->routine(tqueue->data);
	}
}