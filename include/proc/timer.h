#ifndef _TIMER_H_
#define _TIMER_H_

#include <stddef.h>
#include <list.h>

/*
 * Timer structure.
 */
struct timer_t {
  uint32_t expires;
  struct task_t *task;
  struct list_head_t list;
};

struct timer_t *create_timer(uint32_t seconds, void (*func)(void *), void *arg);
void destroy_timer(struct timer_t *timer);

#endif
