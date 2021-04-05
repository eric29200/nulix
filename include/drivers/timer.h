#ifndef _TIMER_H_
#define _TIMER_H_

#include <stddef.h>
#include <list.h>

extern volatile uint32_t jiffies;
extern uint32_t startup_time;

#define HZ              100
#define CURRENT_TIME    (startup_time + (jiffies / HZ))

/*
 * Timer event structure.
 */
struct timer_event_t {
  uint32_t jiffies;
  void (*handler)(void *);
  void *handler_args;
  struct list_t list;
};

void init_timer();
int timer_add_event(uint32_t sec, void (*handler)(void *), void *handler_args);

#endif
