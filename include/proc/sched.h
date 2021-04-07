#ifndef _SCHED_H_
#define _SCHED_H_

int init_scheduler();
void schedule();
int start_thread(void (*func)(void));

#endif
