#include <proc/timer.h>
#include <proc/sched.h>
#include <stderr.h>
#include <stdio.h>

/* timers list */
static LIST_HEAD(timers_list);

/*
 * Init a timer.
 */
void init_timer(struct timer_event *timer, void (*func)(void *), void *data, time_t expires)
{
	INIT_LIST_HEAD(&timer->list);
	timer->expires = expires;
	timer->func = func;
	timer->data = data;
}

/*
 * Add a timer.
 */
void add_timer(struct timer_event *timer)
{
	list_add(&timer->list, &timers_list);
}

/*
 * Delete a timer.
 */
void del_timer(struct timer_event *timer)
{
	if (timer_pending(timer))
		list_del(&timer->list);
}

/*
 * Modify a timer.
 */
void mod_timer(struct timer_event *timer, time_t expires)
{
	del_timer(timer);
	timer->expires = expires;
	add_timer(timer);
}

/*
 * Update all timers.
 */
void update_timers()
{
	struct list_head *pos, *n;
	struct timer_event *timer;

	list_for_each_safe(pos, n, &timers_list) {
		timer = list_entry(pos, struct timer_event, list);

		/* timer expires : run and remove it */
		if (timer->expires <= jiffies) {
			del_timer(timer);
			timer->func(timer->data);
		}
	}
}

/*
 * Nano sleep system call.
 */
int sys_nanosleep(const struct old_timespec *req, struct old_timespec *rem)
{
	time_t expire;

	/* check request */
	if (req->tv_nsec < 0 || req->tv_sec < 0)
		return -EINVAL;

	/* compute delay in jiffies */
	expire = old_timespec_to_jiffies(req) + (req->tv_sec || req->tv_nsec);

	/* sleep */
	current_task->state = TASK_SLEEPING;
	expire = schedule_timeout(expire);

	/* task interrupted before timer end */
	if (expire) {
		if (rem)
			jiffies_to_old_timespec(expire, rem);

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
	if (current_task->real_timer.list.next) {
		val = current_task->real_timer.expires - jiffies;
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
	kill_proc(*((pid_t *) arg), SIGALRM, 1);
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
	del_timer(&current_task->real_timer);

	/* set timer */
	if (new_value->it_value_sec || new_value->it_value_usec) {
		init_timer(&current_task->real_timer, itimer_handler, &current_task->pid, jiffies + ms_to_jiffies(expires_ms));
		add_timer(&current_task->real_timer);
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
