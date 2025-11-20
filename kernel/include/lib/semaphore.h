
#ifndef _SEMAPHORE_H_
#define _SEMAPHORE_H_

#include <proc/sched.h>

/*
 * Semaphore structure.
 */
struct semaphore {
	int 			count;
	struct wait_queue *	wait;
};

/*
 * Init a semaphore.
 */
static inline void init_semaphore(struct semaphore *sem, int count)
{
	sem->count = count;
	sem->wait = NULL;
}

/*
 * Release a semaphore.
 */
static inline void up(struct semaphore *sem)
{
	if (!sem)
		return;

	sem->count++;
	wake_up(&sem->wait);
}

/*
 * Acquire a semaphore.
 */
static inline void down(struct semaphore *sem)
{
	if (!sem)
		return;

	for (;;) {
		if (sem->count > 0) {
			sem->count--;
			return;
		}

		sleep_on(&sem->wait);
	}
}

#endif
