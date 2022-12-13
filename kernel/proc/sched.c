#include <x86/system.h>
#include <x86/interrupt.h>
#include <x86/tss.h>
#include <proc/sched.h>
#include <proc/task.h>
#include <proc/timer.h>
#include <sys/syscall.h>
#include <lib/list.h>
#include <kernel_stat.h>
#include <stderr.h>

LIST_HEAD(tasks_list);					/* active processes list */
static struct task_t *kinit_task;			/* kernel init task (pid = 0) */
struct task_t *init_task;				/* user init task (pid = 1) */
struct task_t *current_task = NULL;			/* current task */
static pid_t next_pid = 1;				/* next pid */
pid_t last_pid = 0;					/* last pid */

struct kernel_stat_t kstat;				/* kernel statistics */

/* switch tasks (defined in scheduler.s) */
extern void scheduler_do_switch(uint32_t *current_esp, uint32_t next_esp);
extern void return_user_mode(struct registers_t *regs);

/* average run */
unsigned long avenrun[3] = { 0, 0, 0 };

/*
 * Get next pid.
 */
pid_t get_next_pid()
{
	last_pid = next_pid++;
	return last_pid;
}

/*
 * Find a task matching pid.
 */
struct task_t *find_task(pid_t pid)
{
	struct list_head_t *pos;
	struct task_t *task;

	list_for_each(pos, &tasks_list) {
		task = list_entry(pos, struct task_t, list);
		if (task->pid == pid)
			return task;
	}

	return NULL;
}

/*
 * Init scheduler.
 */
int init_scheduler(void (*kinit_func)())
{
	/* reset kernel stats */
	memset(&kstat, 0, sizeof(struct kernel_stat_t));

	/* create init task */
	kinit_task = create_kernel_thread(kinit_func, NULL);
	if (!kinit_task)
		return -ENOMEM;

	return 0;
}

/*
 * Get next task to run.
 */
static struct task_t *get_next_task()
{
	struct list_head_t *pos;
	struct task_t *task;

	/* first scheduler call : return kinit */
	if (!current_task)
		return kinit_task;

	/* get next running task */
	list_for_each(pos, &current_task->list) {
		if (pos == &tasks_list)
			continue;

		task = list_entry(pos, struct task_t, list);
		if (task->state == TASK_RUNNING)
			return task;
	}

	/* no tasks found : return current if still running */
	if (current_task->state == TASK_RUNNING)
		return current_task;

	/* else execute kinit */
	return kinit_task;
}

/*
 * Spawn init process.
 */
int spawn_init()
{
	init_task = create_init_task(kinit_task);
	if (!init_task)
		return -ENOMEM;

	return 0;
}

/*
 * Schedule function (interruptions must be disabled and will be reenabled on function return).
 */
void schedule()
{
	struct task_t *prev_task, *task;
	struct list_head_t *pos;

	/* update timers */
	timer_update();

	/* update timeout on all tasks and wake them if needed */
	list_for_each(pos, &tasks_list) {
		task = list_entry(pos, struct task_t, list);
		if (task && task->timeout && task->timeout < jiffies) {
			task->timeout = 0;
			task->state = TASK_RUNNING;
		}
	}

	/* get next task to run */
	prev_task = current_task;
	current_task = get_next_task();

	/* switch tasks */
	if (prev_task != current_task) {
		kstat.context_switch++;
		tss_set_stack(0x10, current_task->kernel_stack);
		load_tls();
		switch_page_directory(current_task->mm->pgd);
		scheduler_do_switch(&prev_task->esp, current_task->esp);
	}

	/* update task time */
	if (current_task)
		current_task->utime++;
}

/*
 * Add to a wait queue.
 */
void add_wait_queue(struct wait_queue_t **wq, struct wait_queue_t *wait)
{
	/* first element */
	if (!*wq) {
		wait->next = wait;
		*wq = wait;
		return;
	}

	wait->next = (*wq)->next;
	(*wq)->next = wait;
}

/*
 * Remove from a wait queue.
 */
void remove_wait_queue(struct wait_queue_t **wq, struct wait_queue_t *wait)
{
	struct wait_queue_t *tmp;

	/* first element */
	if (*wq == wait) {
		*wq = wait->next;
		if (*wq == wait)
			*wq = NULL;

		return;
	}

	/* else find position */
	tmp = wait;
	while (tmp->next != wait)
		tmp = tmp->next;
	tmp->next = wait->next;

	/* reset next element */
	wait->next = NULL;
}

/*
 * Add wait queue to select table.
 */
void select_wait(struct wait_queue_t **wait_address, struct select_table_t *st)
{
	struct select_table_entry_t *entry;

	if (!st || !wait_address)
		return;

	if (st->nr >= MAX_SELECT_TABLE_ENTRIES)
		return;

	/* set new select entry */
	entry = st->entry + st->nr;
	entry->wait_address = wait_address;
	entry->wait.task = current_task;
	entry->wait.next = NULL;
	st->nr++;

	/* add wait queue */
	add_wait_queue(wait_address, &entry->wait);
}

/*
 * Sleep on a wait queue.
 */
void task_sleep(struct wait_queue_t **wq)
{
	struct wait_queue_t wait = { current_task, NULL };

	/* set task's state */
	current_task->state = TASK_SLEEPING;

	/* add to wait queue */
	add_wait_queue(wq, &wait);

	/* reschedule */
	schedule();

	/* remove from wait queue */
	remove_wait_queue(wq, &wait);
}

/*
 * Wake up one task sleeping on a wait queue.
 */
void task_wakeup(struct wait_queue_t **wq)
{
	struct wait_queue_t *tmp;

	/* check first task */
	if (!wq)
		return;
	tmp = *wq;
	if (!tmp)
		return;

	/* wake up first sleeping task */
	do {
		if (tmp->task->state == TASK_SLEEPING) {
			tmp->task->state = TASK_RUNNING;
			break;
		}

		tmp = tmp->next;
	} while (tmp != *wq && tmp != NULL);
}

/*
 * Wake up all tasks sleeping on a wait queue.
 */
void task_wakeup_all(struct wait_queue_t **wq)
{
	struct wait_queue_t *tmp;

	if (!wq)
		return;

	tmp = *wq;
	if (!tmp)
		return;

	/* wake up all tasks */
	do {
		if (tmp->task->state == TASK_SLEEPING)
			tmp->task->state = TASK_RUNNING;

		tmp = tmp->next;
	} while (tmp != *wq);
}

/*
 * Get task with a pid.
 */
struct task_t *get_task(pid_t pid)
{
	struct list_head_t *pos;
	struct task_t *task;

	list_for_each(pos, &tasks_list) {
		task = list_entry(pos, struct task_t, list);
		if (task->pid == pid)
			return task;
	}

	return NULL;
}

/*
 * Send a signal to a task.
 */
static void __task_signal(struct task_t *task, int sig)
{
	/* just check permission */
	if (sig == 0)
		return;

	/* add to pending signals */
	sigaddset(&task->sigpend, sig);

	/* wakeup process if sleeping */
	if (task->state == TASK_SLEEPING || task->state == TASK_STOPPED)
		task->state = TASK_RUNNING;
}

/*
 * Send a signal to a task.
 */
int task_signal(pid_t pid, int sig)
{
	struct task_t *task;

	/* get task */
	task = get_task(pid);
	if (!task)
		return -ESRCH;

	/* send signal */
	__task_signal(task, sig);

	return 0;
}

/*
 * Send a signal to all tasks in a group.
 */
int task_signal_group(pid_t pgrp, int sig)
{
	struct list_head_t *pos;
	struct task_t *task;
	int ret = -ESRCH;

	list_for_each(pos, &tasks_list) {
		task = list_entry(pos, struct task_t, list);
		if (task->pgrp == pgrp) {
			__task_signal(task, sig);
			ret = 0;
		}
	}

	return ret;
}

/*
 * Send a signal to all tasks (except init process).
 */
int task_signal_all(int sig)
{
	struct list_head_t *pos;
	struct task_t *task;

	list_for_each(pos, &tasks_list) {
		task = list_entry(pos, struct task_t, list);
		if (task->pid > 1)
			__task_signal(task, sig);
	}

	return 0;
}

/*
 * Signal return trampoline (this code is executed in user mode).
 */
static int sigreturn()
{
	int ret;

	/* call system call register (= go in kernel mode to change interrupt register and go back in previous user code) */
	__asm__ __volatile__("int $0x80" : "=a" (ret) : "0" (__NR_sigreturn));

	return ret;
}

/*
 * Handle signal of current task.
 */
int do_signal(struct registers_t *regs)
{
	struct sigaction_t *act;
	uint32_t *esp;
	int sig;

	/* get first unblocked signal */
	for (sig = 0; sig < NSIGS; sig++)
		if (sigismember(&current_task->sigpend, sig) && !sigismember(&current_task->sigmask, sig))
			break;

	/* no signal */
	if (sig == NSIGS)
		goto out;

	/* remove signal from current task */
	sigdelset(&current_task->sigpend, sig);
	act = &current_task->sig->action[sig - 1];

	/* ignore signal handler */
	if (act->sa_handler == SIG_IGN)
		goto out;

	/* default signal handler */
	if (act->sa_handler == SIG_DFL) {
		switch (sig) {
			/* ignore those signals */
			case SIGCONT:
			case SIGCHLD:
			case SIGWINCH:
				goto out;
			case SIGSTOP:
			case SIGTSTP:
				current_task->state = TASK_STOPPED;
				current_task->exit_code = sig;
				task_wakeup_all(&current_task->parent->wait_child_exit);
				goto out;
			default:
				sys_exit(sig);
				goto out;
		}
	}

	/* signal in system call : restore user registers */
	if (current_task->in_syscall)
		memcpy(regs, &current_task->user_regs, sizeof(struct registers_t));

	/* save interrupt registers, to restore it at the end of signal */
	memcpy(&current_task->signal_regs, regs, sizeof(struct registers_t));

	/* prepare a stack for signal handler */
	esp = (uint32_t *) regs->useresp;
	*(--esp) = sig;
	*(--esp) = (uint32_t) sigreturn;

	/* changer interrupt registers to return back in signal handler */
	regs->useresp = (uint32_t) esp;
	regs->eip = (uint32_t) act->sa_handler;

out:
	/* restore sigmask */
	if (current_task->saved_sigmask) {
		current_task->sigmask = current_task->saved_sigmask;
		current_task->saved_sigmask = 0;
	}

	/* signal in system call : return to user mode */
	if (current_task->in_syscall) {
		current_task->in_syscall = 0;
		regs->eax = -EINTR;
		return_user_mode(regs);
	}

	return 0;
}
