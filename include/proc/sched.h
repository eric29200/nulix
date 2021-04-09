#ifndef _SCHED_H_
#define _SCHED_H_

#include <proc/task.h>
#include <proc/timer.h>

int init_scheduler(void (*init_func)(void *), void *init_arg);
void schedule();
void schedule_timeout(uint32_t timeout);

int run_task(struct task_t *task);
void kill_task(struct task_t *task);

int run_timer(struct timer_t *timer);

#endif
