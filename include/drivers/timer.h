#ifndef _TIMER_H_
#define _TIMER_H_

#include <stddef.h>

extern volatile uint32_t jiffies;
extern uint32_t startup_time;

#define HZ              100
#define CURRENT_TIME    (startup_time + (jiffies / HZ))

void init_timer();

#endif
