#include <ipc/semaphore.h>
#include <proc/sched.h>

/*
 * Init a seamphore.
 */
void init_sem(struct semaphore_t *sem)
{
  sem->count = 1;
  spin_lock_init(&sem->lock);
  init_waitqueue_head(&sem->wait);
}

/*
 * Acquire a semaphore.
 */
void sem_down(struct semaphore_t *sem)
{
  uint32_t flags;

  while (1) {
    /* lock semaphore */
    spin_lock_irqsave(&sem->lock, flags);

    /* semaphore free : return */
    if (sem->count > 0) {
      sem->count--;
      spin_unlock_irqrestore(&sem->lock, flags);
      return;
    }

    /* otherwise wait */
    spin_unlock_irqrestore(&sem->lock, flags);
    wait(&sem->wait);
  }
}

/*
 * Release a semaphore.
 */
void sem_up(struct semaphore_t *sem)
{
  uint32_t flags;

  /* lock semaphore */
  spin_lock_irqsave(&sem->lock, flags);

  sem->count = 1;
  wake_up(&sem->wait);

  /* release semaphore */
  spin_unlock_irqrestore(&sem->lock, flags);
}
