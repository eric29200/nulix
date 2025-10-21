#include <proc/timer.h>
#include <proc/sched.h>
#include <stderr.h>
#include <stdio.h>

/* timers list */
static LIST_HEAD(timers_list);

/*
 * Init a timer event.
 */
void timer_event_init(struct timer_event *tm, void (*func)(void *), void *data, time_t expires)
{
	INIT_LIST_HEAD(&tm->list);
	tm->expires = expires;
	tm->func = func;
	tm->data = data;
}

/*
 * Add a timer event.
 */
void timer_event_add(struct timer_event *tm)
{
	list_add(&tm->list, &timers_list);
}

/*
 * Delete a timer event.
 */
void timer_event_del(struct timer_event *tm)
{
	list_del(&tm->list);
}

/*
 * Modify a timer event.
 */
void timer_event_mod(struct timer_event *tm, time_t expires)
{
	tm->expires = expires;
	timer_event_add(tm);
}

/*
 * Update all timers.
 */
void update_timers()
{
	struct list_head *pos, *n;
	struct timer_event *tm;

	list_for_each_safe(pos, n, &timers_list) {
		tm = list_entry(pos, struct timer_event, list);

		/* timer expires : run and remove it */
		if (tm->expires <= jiffies) {
			timer_event_del(tm);
			tm->func(tm->data);
		}
	}
}

/*
 * Nano sleep system call.
 */
int sys_nanosleep(const struct old_timespec *req, struct old_timespec *rem)
{
	time_t timeout;

	/* check request */
	if (req->tv_nsec < 0 || req->tv_sec < 0)
		return -EINVAL;

	/* compute delay in jiffies */
	timeout = old_timespec_to_jiffies(req) + (req->tv_sec || req->tv_nsec) + jiffies;

	/* set current state sleeping and set timeout */
	current_task->state = TASK_SLEEPING;
	current_task->timeout = timeout;

	/* reschedule */
	schedule();

	/* task interrupted before timer end */
	if (timeout > jiffies) {
		if (rem)
			jiffies_to_old_timespec(timeout - jiffies - (timeout > jiffies + 1), rem);

		return -EINTR;
	}

	return 0;
}

/*
 * Get current timer.
 */
static int do_getitimer(int which, struct itimerval *value)
{
	uint32_t val;

	/* implement only real timer */
	if (which != ITIMER_REAL) {
		printf("setitimer (%d) not implemented\n", which);
		return -ENOSYS;
	}

	/* get current timer */
	if (current_task->sig_tm.list.next) {
		val = current_task->sig_tm.expires - jiffies;
		if ((long ) val <= 0)
			val = 1;
	}

	/* set value */
	value->it_interval_sec = 0;
	value->it_interval_usec = 0;
	value->it_value_sec = val / HZ;
	value->it_value_usec = (jiffies % HZ) * (1000000 / HZ);

	return 0;
}

/*
 * Timer handler = send a SIGALRM signal to caller.
 */
static void itimer_handler(void *arg)
{
	kill_proc(*((pid_t *) arg), SIGALRM);
}

/*
 * Set an interval timer (send SIGALRM signal at expiration).
 */
int sys_setitimer(int which, const struct itimerval *new_value, struct itimerval *old_value)
{
	uint32_t expires_ms;
	int ret;

	/* implement only real timer */
	if (which != ITIMER_REAL) {
		printf("setitimer (%d) not implemented\n", which);
		return -ENOSYS;
	}

	/* get old timer */
	if (old_value) {
		ret = do_getitimer(which, old_value);
		if (ret < 0)
			return ret;
	}

	/* compute expiration in ms */
	expires_ms = new_value->it_value_sec * 1000;
	expires_ms += (new_value->it_value_usec / 1000);

	/* delete timer */
	if (current_task->sig_tm.list.next)
		timer_event_del(&current_task->sig_tm);

	/* set timer */
	if (new_value->it_value_sec || new_value->it_value_usec) {
		timer_event_init(&current_task->sig_tm, itimer_handler, &current_task->pid, jiffies + ms_to_jiffies(expires_ms));
		timer_event_add(&current_task->sig_tm);
	}

	return 0;
}

/*
 * Alarm system call.
 */
int sys_alarm(uint32_t seconds)
{
	struct itimerval it_new, it_old;
	uint32_t oldalarm;

	/* set timer */
	it_new.it_interval_sec = 0;
	it_new.it_interval_usec = 0;
	it_new.it_value_sec = seconds;
	it_new.it_value_usec = 0;
	sys_setitimer(ITIMER_REAL, &it_new, &it_old);

	/* set old alarm */
	oldalarm = it_old.it_value_sec;
	if (it_old.it_value_usec)
		oldalarm++;

	return oldalarm;
}
