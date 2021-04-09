#ifndef _WAIT_H_
#define _WAIT_H_

#include <proc/task.h>
#include <stddef.h>
#include <list.h>
#include <lock.h>

/*
 * Wait queue entry.
 */
struct wait_queue_t {
  struct task_t *task;
  struct list_head_t list;
};

/*
 * Wait queue list.
 */
struct wait_queue_head_t {
  spinlock_t lock;
  struct list_head_t task_list;
};

/*
 * Init a wait queue.
 */
static inline void init_waitqueue_head(struct wait_queue_head_t *q)
{
  if (!q)
    return;

  spin_lock_init(&q->lock);
  INIT_LIST_HEAD(&q->task_list);
}

/*
 * Init a wait queue entry.
 */
static inline void init_waitqueue_entry(struct wait_queue_t *q, struct task_t *t)
{
  if (!q || !t)
    return;

  q->task = t;
  INIT_LIST_HEAD(&q->list);
}

void add_wait_queue(struct wait_queue_head_t *q, struct wait_queue_t *wait);
void remove_wait_queue(struct wait_queue_head_t *q, struct wait_queue_t *wait);

#endif
