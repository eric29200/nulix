#ifndef _SCHED_H_
#define _SCHED_H_

#include <proc/task.h>

#define TASK_RETURN_ADDRESS		0xFFFFFFFF

extern struct task_t *init_task;
extern struct task_t *current_task;
extern struct list_head_t tasks_list;

int init_scheduler(void (*kinit_func)());
int spawn_init();
struct task_t *find_task(pid_t pid);
pid_t get_next_pid();
void schedule();

void task_sleep(void *chan);
void task_wakeup(void *chan);
void task_wakeup_all(void *chan);

int task_signal(pid_t pid, int sig);
int task_signal_group(pid_t pgid, int sig);
int task_signal_all(int sig);

int do_signal(struct registers_t *regs);

#endif
