#ifndef _SCHED_H_
#define _SCHED_H_

#include <proc/task.h>
#include <proc/wait.h>

int init_scheduler(void (*init_func)(void *), void *init_arg);
void schedule();
void schedule_timeout(uint32_t timeout);

int run_task(struct task_t *task);
void kill_task(struct task_t *task);

void wait(struct wait_queue_head_t *q);
void wake_up(struct wait_queue_head_t *q);

#endif
