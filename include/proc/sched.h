#ifndef _SCHED_H_
#define _SCHED_H_

#include <proc/task.h>

int init_scheduler(void (*init_func)(void *), void *init_arg);
void schedule();
int run_task(struct task_t *task);
void kill_task(struct task_t *task);

#endif
