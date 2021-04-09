#include <proc/timer.h>
#include <proc/task.h>
#include <drivers/pit.h>
#include <mm/mm.h>

/*
 * Create a timer.
 */
struct timer_t *create_timer(uint32_t seconds, void (*func)(void *), void *arg)
{
  struct timer_t *timer;

  /* create timer */
  timer = (struct timer_t *) kmalloc(sizeof(struct timer_t));
  if (!timer)
    return NULL;

  /* set timer */
  timer->expires = seconds * HZ;
  timer->task = create_task(func, arg);
  if (!timer->task) {
    kfree(timer);
    return NULL;
  }

  timer->task->expires = timer->expires;
  INIT_LIST_HEAD(&timer->list);

  return timer;
}

/*
 * Destroy a timer.
 */
void destroy_timer(struct timer_t *timer)
{
  if (!timer)
    return;

  destroy_task(timer->task);
  kfree(timer);
}
