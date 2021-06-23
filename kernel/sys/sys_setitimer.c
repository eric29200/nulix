#include <sys/syscall.h>
#include <proc/timer.h>
#include <ipc/signal.h>
#include <stdio.h>
#include <stderr.h>

/*
 * Timer handler = send a SIGALRM signal to caller.
 */
static void itimer_handler(void *arg)
{
  pid_t *pid = (pid_t *) arg;
  task_signal(*pid, SIGALRM);
}

/*
 * Set an interval timer (send SIGALRM signal at expiration).
 */
int sys_setitimer(int which, const struct itimerval_t *new_value, struct itimerval_t *old_value)
{
  uint32_t expires_ms;

  /* unused old value */
  UNUSED(old_value);

  /* implement only real timer */
  if (which != ITIMER_REAL) {
    printf("setitimer (%d) not implemented\n", which);
    return -ENOSYS;
  }

  /* compute expiration in ms */
  expires_ms = new_value->it_value_sec * 1000;
  expires_ms += (new_value->it_value_usec / 1000);

  /* set timer */
  timer_event_init(&current_task->sig_tm, itimer_handler, &current_task->pid, jiffies + ms_to_jiffies(expires_ms));
  timer_event_add(&current_task->sig_tm);

  return 0;
}
