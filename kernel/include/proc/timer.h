#ifndef _TIMER_H_
#define _TIMER_H_

#include <lib/list.h>
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
void timer_update();

#endif
