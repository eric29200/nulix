#ifndef _SCHED_H_
#define _SCHED_H_

#include <proc/task.h>

int init_scheduler(void (*init_func)(void));
void schedule();
int start_task(void (*func)(void));
void end_task(struct task_t *task);

#endif
