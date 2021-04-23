#ifndef _SCHED_H_
#define _SCHED_H_

#include <proc/task.h>
#include <proc/wait.h>

#define TASK_RETURN_ADDRESS               0xFFFFFFFF

int init_scheduler(void (*init_func)());
void schedule();
void schedule_timeout(uint32_t timeout);
pid_t get_next_pid();

int run_task(struct task_t *task);
void kill_task(struct task_t *task);

void wait(struct wait_queue_head_t *q);
void wake_up(struct wait_queue_head_t *q);
void wake_up_all(struct wait_queue_head_t *q);

extern struct task_t *current_task;

#endif
