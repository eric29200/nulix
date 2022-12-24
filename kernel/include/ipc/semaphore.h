
#ifndef _SEMAPHORE_H_
#define _SEMAPHORE_H_

#include <proc/sched.h>

/*
 * Semaphore structure.
 */
struct semaphore_t {
	int count;
	struct wait_queue_t *wait;
};

/*
 * Init a semaphore.
 */
static inline void init_semaphore(struct semaphore_t *sem, int count)
{
	sem->count = count;
	sem->wait = NULL;
}

/*
 * Release a semaphore.
 */
static inline void up(struct semaphore_t *sem)
{
	if (!sem)
		return;

	sem->count++;
	task_wakeup(&sem->wait);
}

/*
 * Acquire a semaphore.
 */
static inline void down(struct semaphore_t *sem)
{
	if (!sem)
		return;

	for (;;) {
		if (sem->count > 0) {
			sem->count--;
			return;
		}

		task_sleep(&sem->wait);
	}
}

#endif
