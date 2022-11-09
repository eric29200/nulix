#ifndef _SCHED_H_
#define _SCHED_H_

#include <proc/task.h>
#include <proc/wait.h>

#define TASK_RETURN_ADDRESS		0xFFFFFFFF

#define FSHIFT				11		/* nr of bits of precision */
#define FIXED_1				(1 << FSHIFT)	/* 1.0 as fixed-point */
#define LOAD_FREQ			(5 * HZ)	/* 5 sec intervals */
#define EXP_1				1884		/* 1/exp(5sec/1min) as fixed-point */
#define EXP_5				2014		/* 1/exp(5sec/5min) */
#define EXP_15				2037		/* 1/exp(5sec/15min) */

#define CLONE_VM			0x00000100	/* set if VM shared between processes */
#define CLONE_FS			0x00000200	/* set if fs info shared between processes */
#define CLONE_FILES			0x00000400	/* set if open files shared between processes */
#define CLONE_SIGHAND			0x00000800	/* set if signal handlers and blocked signals shared */
#define CLONE_PTRACE			0x00002000	/* set if we want to let tracing continue on the child too */
#define CLONE_VFORK			0x00004000	/* set if the parent wants the child to wake it up on mm_release */
#define CLONE_PARENT			0x00008000	/* set if we want to have the same parent as the cloner */
#define CLONE_THREAD			0x00010000	/* Same thread group? */
#define CLONE_NEWNS			0x00020000	/* New namespace group? */

#define CALC_LOAD(load, exp,n ) \
	load *= exp; \
	load += n*(FIXED_1-exp); \
	load >>= FSHIFT;

/*
 * Check if a process has pending signals.
 */
static inline int signal_pending(struct task_t *task)
{
	return task->sigpend & ~(task->sigmask);
}

extern unsigned long avenrun[];				/* Load averages */

extern struct task_t *init_task;
extern struct task_t *current_task;
extern struct list_head_t tasks_list;
extern pid_t last_pid;

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
int task_signal_group(pid_t pgrp, int sig);
int task_signal_all(int sig);

int do_signal(struct registers_t *regs);

#endif
