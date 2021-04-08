#ifndef _SCHED_H_
#define _SCHED_H_

#include <proc/task.h>

int init_scheduler(void (*init_func)(void));
void schedule();
int start_thread(void (*func)(void));
void end_thread(struct thread_t *thread);

#endif
