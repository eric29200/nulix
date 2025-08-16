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

void timer_event_init(struct timer_event *tm, void (*func)(void *), void *data, time_t expires);
void timer_event_add(struct timer_event *tm);
void timer_event_del(struct timer_event *tm);
void timer_event_mod(struct timer_event *tm, time_t expires);
void update_timers();

int sys_nanosleep(const struct old_timespec *req, struct old_timespec *rem);
int sys_setitimer(int which, const struct itimerval *new_value, struct itimerval *old_value);
int sys_alarm(uint32_t seconds);

#endif
