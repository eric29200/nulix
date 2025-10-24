#ifndef _TIMER_H_
#define _TIMER_H_

#include <lib/list.h>
#include <time.h>
#include <stddef.h>

/*
 * Timer event structure.
 */
struct timer_event {
	time_t			expires;
	void			(*func)(void *);
	void *			data;
	struct list_head	list;
};

void init_timer(struct timer_event *timer, void (*func)(void *), void *data, time_t expires);
void add_timer(struct timer_event *timer);
void del_timer(struct timer_event *timer);
void mod_timer(struct timer_event *timer, time_t expires);
void update_timers();

int sys_nanosleep(const struct old_timespec *req, struct old_timespec *rem);
int sys_setitimer(int which, const struct itimerval *new_value, struct itimerval *old_value);
int sys_alarm(uint32_t seconds);

/*
 * Is a timer pending ?
 */
static inline int timer_pending(struct timer_event *timer)
{
	return timer->list.next != NULL;
}

#endif
