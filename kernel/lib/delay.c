#include <delay.h>
#include <drivers/pit.h>
#include <proc/sched.h>
#include <proc/timer.h>

/*
 * Msleep timer handler.
 */
static void msleep_timer_handler(void *data)
{
  struct task_t *task = (struct task_t *) data;
  task->state = TASK_RUNNING;
}

/*
 * Sleep in milliseonds.
 */
void msleep(uint32_t msecs)
{
  struct timer_event_t tm;

  /* set current state sleeping */
  current_task->state = TASK_SLEEPING;

  /* init a timer */
  timer_event_init(&tm, msleep_timer_handler, current_task, jiffies + ms_to_jiffies(msecs));
  timer_event_add(&tm);

  /* reschedule */
  schedule();
}
