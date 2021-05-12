#include <proc/timer.h>
#include <proc/sched.h>

/* timers list */
static LIST_HEAD(timers_list);

/*
 * Init a timer event.
 */
void timer_event_init(struct timer_event_t *tm, void (*func)(void *), void *data, uint32_t expires)
{
  INIT_LIST_HEAD(&tm->list);
  tm->expires = expires;
  tm->func = func;
  tm->data = data;
}

/*
 * Add a timer event.
 */
void timer_event_add(struct timer_event_t *tm)
{
  list_add(&tm->list, &timers_list);
}

/*
 * Delete a timer event.
 */
void timer_event_del(struct timer_event_t *tm)
{
  list_del(&tm->list);
}

/*
 * Modify a timer event.
 */
void timer_event_mod(struct timer_event_t *tm, uint32_t expires)
{
  tm->expires = expires;
  timer_event_add(tm);
}

/*
 * Update all timers.
 */
void timer_update()
{
  struct list_head_t *pos, *n;
  struct timer_event_t *tm;

  list_for_each_safe(pos, n, &timers_list) {
    tm = list_entry(pos, struct timer_event_t, list);

    /* timer expires : run and remove it */
    if (tm->expires <= jiffies) {
      timer_event_del(tm);
      tm->func(tm->data);
    }
  }
}
