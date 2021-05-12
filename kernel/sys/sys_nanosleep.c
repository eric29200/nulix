#include <sys/syscall.h>
#include <proc/timer.h>
#include <proc/sched.h>
#include <stderr.h>

/*
 * Sleep timer handler.
 */
static void sleep_timer_handler(void *data)
{
  struct task_t *task = (struct task_t *) data;
  task->state = TASK_RUNNING;
}

/*
 * Nano sleep system call.
 */
int sys_nanosleep(const struct timespec_t *req, struct timespec_t *rem)
{
  struct timer_event_t tm;
  uint32_t expire;

  /* check request */
  if (req->tv_nsec >= 1000000000L || req->tv_nsec < 0 || req->tv_sec < 0)
    return -EINVAL;

  /* compute delay in jiffies */
  expire = timespec_to_jiffies(req) + (req->tv_sec || req->tv_nsec) + jiffies;

  /* set current state sleeping */
  current_task->state = TASK_SLEEPING;

  /* init a timer */
  timer_event_init(&tm, sleep_timer_handler, current_task, expire);
  timer_event_add(&tm);

  /* reschedule */
  schedule();

  /* task interrupted before timer end */
  if (expire > jiffies) {
    /* remove timer */
    timer_event_del(&tm);

    if (rem)
      jiffies_to_timespec(expire - jiffies - (expire > jiffies + 1), rem);

    return -EINTR;
  }

  return 0;
}
