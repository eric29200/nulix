#include <x86/system.h>
#include <x86/interrupt.h>
#include <x86/tss.h>
#include <drivers/char/pit.h>
#include <proc/sched.h>
#include <proc/task.h>
#include <proc/timer.h>
#include <sys/syscall.h>
#include <lib/list.h>
#include <kernel_stat.h>
#include <stdio.h>
#include <stderr.h>

LIST_HEAD(tasks_list);					/* active processes list */
static struct task *kinit_task;				/* kernel init task (pid = 0) */
struct task *init_task;					/* user init task (pid = 1) */
struct task *current_task = NULL;			/* current task */
static pid_t next_pid = 0;				/* next pid */
pid_t last_pid = 0;					/* last pid */

struct kernel_stat kstat;				/* kernel statistics */

/* switch tasks (defined in scheduler.s) */
extern void scheduler_do_switch(uint32_t *current_esp, uint32_t next_esp);
extern void return_user_mode(struct registers *regs);

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
struct task *find_task(pid_t pid)
{
	struct list_head *pos;
	struct task *task;

	list_for_each(pos, &tasks_list) {
		task = list_entry(pos, struct task, list);
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
	memset(&kstat, 0, sizeof(struct kernel_stat));

	/* create init task */
	kinit_task = create_kinit_task(kinit_func);
	if (!kinit_task)
		return -ENOMEM;

	/* switch to kinit */
	current_task = kinit_task;
	tss_set_stack(0x10, current_task->kernel_stack);
	load_tls();
	switch_page_directory(current_task->mm->pgd);
	scheduler_do_switch(0, current_task->esp);

	return 0;
}

/*
 * Spawn init process.
 */
int spawn_init()
{
	/* create init */
	init_task = create_init_task(kinit_task);
	if (!init_task)
		return -ENOMEM;

	return 0;
}

/*
 * Update current process times.
 */
static void update_process_times()
{
	if (current_task && current_task->pid)
		current_task->utime++;
}

/*
 * Handle timer (PIT) interrupt.
 */
void do_timer_interrupt()
{
	/* update times and timers */
	update_times();
	update_process_times();
	update_timers();

	/* schedule */
	schedule();
}

/*
 * Process a task timeout.
 */
static void process_timeout(void *arg)
{
	struct task *task = (struct task *) arg;

	task->timeout = 0;
	task->state = TASK_RUNNING;
}

/*
 * Get next task to run.
 */
static struct task *get_next_task()
{
	struct list_head *pos;
	struct task *task;

	/* first scheduler call : return kinit */
	if (!current_task)
		return kinit_task;

	/* get next running task */
	list_for_each(pos, &current_task->list) {
		if (pos == &tasks_list)
			continue;

		task = list_entry(pos, struct task, list);
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
 * Schedule function (interruptions must be disabled and will be reenabled on function return).
 */
void schedule()
{
	struct task *prev, *next;

	/* save current task */
	prev = current_task;

	/* previous task sleeping on a timeout : create a timer */
	if (prev->state == TASK_SLEEPING && prev->timeout) {
		/* delete previous timer */
		if (prev->timeout_tm.list.next)
			timer_event_del(&current_task->timeout_tm);

		/* create new timer */
		timer_event_init(&prev->timeout_tm, process_timeout, prev, prev->timeout);
		timer_event_add(&prev->timeout_tm);
		prev->timeout = 0;
	}

	/* get next task to run */
	next = get_next_task();

	/* switch tasks */
	if (prev != next) {
		/* update kernel stats */
		kstat.context_switch++;

		/* real switch */
		current_task = next;
		tss_set_stack(0x10, current_task->kernel_stack);
		load_tls();
		switch_page_directory(current_task->mm->pgd);
		scheduler_do_switch(&prev->esp, current_task->esp);
	}
}

/*
 * Add to a wait queue.
 */
void add_wait_queue(struct wait_queue **wq, struct wait_queue *wait)
{
	struct wait_queue *next = WAIT_QUEUE_HEAD(wq);
	struct wait_queue *head = *wq;

	if (head)
		next = head;

	*wq = wait;
	wait->next = next;
}

/*
 * Remove from a wait queue.
 */
void remove_wait_queue(struct wait_queue *wait)
{
	struct wait_queue *next = wait->next;
	struct wait_queue *head = next;

	for (;;) {
		struct wait_queue *nextlist = head->next;

		if (nextlist == wait)
			break;

		head = nextlist;
	}

	head->next = next;
}

/*
 * Add wait queue to select table.
 */
void select_wait(struct wait_queue **wait_address, struct select_table *st)
{
	struct select_table_entry *entry;

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
void sleep_on(struct wait_queue **wq)
{
	struct wait_queue wait = { current_task, NULL };

	/* set task's state */
	current_task->state = TASK_SLEEPING;

	/* add to wait queue */
	add_wait_queue(wq, &wait);

	/* reschedule */
	schedule();

	/* remove from wait queue */
	remove_wait_queue(&wait);
}

/*
 * Wake up all tasks sleeping on a wait queue.
 */
void wake_up(struct wait_queue **wq)
{
	struct wait_queue *tmp;

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
	} while (tmp != *wq && tmp != NULL);
}

/*
 * Get task with a pid.
 */
struct task *get_task(pid_t pid)
{
	struct list_head *pos;
	struct task *task;

	list_for_each(pos, &tasks_list) {
		task = list_entry(pos, struct task, list);
		if (task->pid == pid)
			return task;
	}

	return NULL;
}

/*
 * Send a signal to a task.
 */
static void __task_signal(struct task *task, int sig)
{
	/* just check permission */
	if (sig == 0)
		return;

	/* add to pending signals */
	sigaddset(&task->sigpend, sig);

	/* wake up process if sleeping */
	if (task->state == TASK_SLEEPING || task->state == TASK_STOPPED)
		task->state = TASK_RUNNING;
}

/*
 * Send a signal to a task.
 */
int task_signal(pid_t pid, int sig)
{
	struct task *task;

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
	struct list_head *pos;
	struct task *task;
	int ret = -ESRCH;

	list_for_each(pos, &tasks_list) {
		task = list_entry(pos, struct task, list);
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
	struct list_head *pos;
	struct task *task;

	list_for_each(pos, &tasks_list) {
		task = list_entry(pos, struct task, list);
		if (task->pid > 1)
			__task_signal(task, sig);
	}

	return 0;
}

/*
 * Get pid system call.
 */
pid_t sys_getpid()
{
	return current_task->pid;
}

/*
 * Get parent pid system call.
 */
pid_t sys_getppid()
{
	if (current_task->parent)
		return current_task->parent->pid;

	/* init process : no father */
	return current_task->pid;
}

/*
 * Get process group id system call.
 */
pid_t sys_getpgid(pid_t pid)
{
	struct task *task;

	/* get matching task or current task */
	if (pid == 0)
		task = current_task;
	else
		task = get_task(pid);

	/* no matching task */
	if (task == NULL)
		return -1;

	/* get process group id */
	return task->pgrp;
}

/*
 * Get thread id system call.
 */
pid_t sys_gettid()
{
	return current_task->pid;
}

/*
 * Get user id system call.
 */
uid_t sys_getuid()
{
	return current_task->uid;
}

/*
 * Get group id system call.
 */
gid_t sys_getgid()
{
	return current_task->gid;
}

/*
 * Get effective group id system call.
 */
gid_t sys_getegid()
{
	return current_task->egid;
}

/*
 * Get effective user id system call.
 */
uid_t sys_geteuid()
{
	return current_task->euid;
}

/*
 * Get session id system call.
 */
int sys_getsid(pid_t pid)
{
	struct task *task;

	/* return current session */
	if (!pid)
		return current_task->session;

	/* return matching process session */
	task = find_task(pid);
	if (task)
		return task->session;

	return -ESRCH;
}

/*
 * Set process group id system call.
 */
int sys_setpgid(pid_t pid, pid_t pgid)
{
	struct task *task;

	/* get matching task or current task */
	if (pid == 0)
		task = current_task;
	else
		task = get_task(pid);

	/* no matching task */
	if (task == NULL)
		return -1;

	/* set process group id (if pgid = 0, set task process group id to its pid */
	if (pgid == 0)
		task->pgrp = task->pid;
	else
		task->pgrp = pgid;

	return 0;
}

/*
 * Set user id system call.
 */
int sys_setuid(uid_t uid)
{
	current_task->uid = uid;
	return 0;
}

/*
 * Set group id system call.
 */
int sys_setgid(gid_t gid)
{
	current_task->gid = gid;
	return 0;
}

/*
 * Set session id system call.
 */
int sys_setsid()
{
	current_task->leader = 1;
	current_task->pgrp = current_task->pid;
	current_task->session = current_task->pid;
	current_task->tty = NULL;

	return current_task->session;
}

/*
 * Set gid system call.
 */
int sys_setresgid(gid_t rgid, gid_t egid, gid_t sgid)
{
	UNUSED(rgid);
	UNUSED(egid);
	UNUSED(sgid);
	return 0;
}

/*
 * Set uid system call.
 */
int sys_setresuid(uid_t ruid, uid_t euid, uid_t suid)
{
	UNUSED(ruid);
	UNUSED(euid);
	UNUSED(suid);
	return 0;
}

/*
 * Set groups system call.
 */
int sys_setgroups(size_t size, const gid_t *list)
{
	UNUSED(size);
	UNUSED(list);
	return 0;
}
