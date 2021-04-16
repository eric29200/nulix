#include <proc/wait.h>
#include <lock.h>

/*
 * Add a wait queue entry.
 */
void add_wait_queue(struct wait_queue_head_t *q, struct wait_queue_t *wait)
{
  uint32_t flags;

  spin_lock_irqsave(&q->lock, flags);
  list_add(&wait->list, &q->task_list);
  spin_unlock_irqrestore(&q->lock, flags);
}

/*
 * Remove a wait queue entry.
 */
void remove_wait_queue(struct wait_queue_head_t *q, struct wait_queue_t *wait)
{
  uint32_t flags;

  spin_lock_irqsave(&q->lock, flags);
  list_del(&wait->list);
  spin_unlock_irqrestore(&q->lock, flags);
}
