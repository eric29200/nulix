#ifndef _SCHED_H_
#define _SCHED_H_

#include <proc/task.h>
#include <proc/wait.h>

#define TASK_RETURN_ADDRESS		0xFFFFFFFF

extern struct task_t *init_task;
extern struct task_t *current_task;
extern struct list_head_t tasks_list;

int init_scheduler(void (*kinit_func)());
int spawn_init();
struct task_t *find_task(pid_t pid);
pid_t get_next_pid();
void schedule();

void add_wait_queue(struct wait_queue_t **wq, struct wait_queue_t *wait);
void remove_wait_queue(struct wait_queue_t **wq, struct wait_queue_t *wait);

void select_wait(struct wait_queue_t **wait_address, struct select_table_t *st);
void task_sleep(struct wait_queue_t **wq);
void task_wakeup(struct wait_queue_t **wq);
void task_wakeup_all(struct wait_queue_t **wq);

int task_signal(pid_t pid, int sig);
int task_signal_group(pid_t pgid, int sig);
int task_signal_all(int sig);

int do_signal(struct registers_t *regs);

#endif
