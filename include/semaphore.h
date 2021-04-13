#ifndef _SEMAPHORE_H_
#define _SEMAPHORE_H_

#include <stddef.h>
#include <lock.h>
#include <proc/wait.h>

/*
 * Semaphore.
 */
struct semaphore_t {
  uint8_t count;
  spinlock_t lock;
  struct wait_queue_head_t wait;
};

void init_sem(struct semaphore_t *sem);
void sem_down(struct semaphore_t *sem);
void sem_up(struct semaphore_t *sem);

#endif
