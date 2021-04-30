#ifndef _SCHED_H_
#define _SCHED_H_

#include <proc/task.h>

#define TASK_RETURN_ADDRESS               0xFFFFFFFF

extern struct task_t *current_task;

int init_scheduler(void (*kinit_func)());
void schedule();
pid_t get_next_pid();

void kill_task(struct task_t *task);

#endif
