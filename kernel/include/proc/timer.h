#ifndef _TIMER_H_
#define _TIMER_H_

#include <stddef.h>
#include <list.h>

/*
 * Timer event structure.
 */
struct timer_event_t {
  uint32_t expires;
  void (*func)(void *);
  void *data;
  struct list_head_t list;
};

void timer_event_init(struct timer_event_t *tm, void (*func)(void *), void *data, uint32_t expires);
void timer_event_add(struct timer_event_t *tm);
void timer_event_del(struct timer_event_t *tm);
void timer_event_mod(struct timer_event_t *tm, uint32_t expires);
void timer_update();

#endif