#include <x86/system.h>
#include <x86/interrupt.h>
#include <x86/gdt.h>
#include <x86/ldt.h>
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
int need_resched = 0;					/* reschedule needed ? */

struct kernel_stat kstat;				/* kernel statistics */

/* switch tasks (defined in scheduler.s) */
extern void scheduler_do_switch(uint32_t *current_esp, uint32_t next_esp);

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
 * Switch to next task.
 */
static void switch_to(struct task *prev, struct task *next)
{
	/* set current task */
	current_task = next;

	/* load current task */
	switch_ldt(prev, next);
	load_tss(current_task->thread.kernel_stack);
	load_tls();
	switch_pgd(current_task->mm->pgd);

	/* switch */
	scheduler_do_switch(prev ? &prev->thread.esp : 0, current_task->thread.esp);
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
	switch_to(NULL, kinit_task);

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

	/* reschedule */
	schedule();

	return 0;
}

/*
 * Update current process times.
 */
static void update_process_times()
{
	/* don't update kinit */
	if (!current_task || !current_task->pid)
		return;

	/* update time */
	current_task->utime++;

	/* update counter */
	current_task->counter--;
	if (current_task->counter <= 0) {
		current_task->counter = 0;
		need_resched = 1;
	}
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

	/* sync buffers on disk */
	bsync();

	/* schedule */
	if (need_resched)
		schedule();
}

/*
 * Wake a process = make it runnable.
 */
static void wake_up_process(struct task *task)
{
	task->state = TASK_RUNNING;
	need_resched = 1;
}

/*
 * Process a task timeout.
 */
static void process_timeout(void *arg)
{
	struct task *task = (struct task *) arg;

	task->timeout = 0;
	wake_up_process(task);
}

/*
 * Decides how desirable a process is.
 */
static int goodness(struct task *task, struct task *prev)
{
	int weight = task->counter;

	/* give a slight advantage to the current process */
	if (task == prev)
		weight++;

	return weight;
}
  
/*
 * Schedule function (interruptions must be disabled and will be reenabled on function return).
 */
void schedule()
{
	struct task *prev, *next, *task;
	int next_counter, weight;
	struct list_head *pos;

	/* save current task */
	prev = current_task;
	need_resched = 0;

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

	/* choose next task */
	next = kinit_task;
	next_counter = -1000;
	list_for_each(pos, &tasks_list) {
		task = list_entry(pos, struct task, list);

		/* skip sleeping tasks */
		if (task->state != TASK_RUNNING)
			continue;

		/* compute goodness */
		weight = goodness(task, prev);
		if (weight > next_counter) {
			next = task;
			next_counter = weight;
		}
	}

	/* if all runnable processes have "counter == 0", re-calculate counters */
	if (!next_counter) {
		list_for_each(pos, &tasks_list) {
			task = list_entry(pos, struct task, list);
			task->counter = (task->counter >> 1) + task->priority;
		}
	}

	/* switch tasks */
	if (prev != next) {
		/* update kernel stats */
		kstat.context_switch++;

		/* real switch */
		switch_to(prev, next);
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
			wake_up_process(tmp->task);

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
		wake_up_process(task);
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
	if (suser())
		current_task->uid = current_task->euid = current_task->suid = current_task->fsuid = uid;
	else if (uid == current_task->uid || uid == current_task->suid)
		current_task->euid = current_task->fsuid = uid;
	else
		return -EPERM;

	return 0;
}

/*
 * Set group id system call.
 */
int sys_setgid(gid_t gid)
{
	if (suser())
		current_task->gid = current_task->egid = current_task->sgid = current_task->fsgid = gid;
	else if (gid == current_task->gid || gid == current_task->sgid)
		current_task->egid = current_task->fsgid = gid;
	else
		return -EPERM;

	return 0;
}

/*
 * Set session id system call.
 */
int sys_setsid()
{
	struct list_head *pos;
	struct task *task;

	list_for_each(pos, &tasks_list) {
		task = list_entry(pos, struct task, list);
		if (task->pgrp == current_task->pid)
			return -EPERM;
	}

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
	/* check permissions */
	if (!suser()) {
		if ((rgid != (gid_t) -1) && (rgid != current_task->gid) && (rgid != current_task->egid) && (rgid != current_task->sgid))
			return -EPERM;
		if ((egid != (gid_t) -1) && (egid != current_task->gid) && (egid != current_task->egid) && (egid != current_task->sgid))
			return -EPERM;
		if ((sgid != (gid_t) -1) && (sgid != current_task->gid) && (sgid != current_task->egid) && (sgid != current_task->sgid))
			return -EPERM;
	}

	if (rgid != (gid_t) -1)
		current_task->gid = rgid;
	if (egid != (gid_t) -1)
		current_task->egid = egid;

	current_task->fsgid = current_task->fsgid;
	if (sgid != (gid_t) -1)
		current_task->sgid = sgid;

	return 0;
}

/*
 * Set uid system call.
 */
int sys_setresuid(uid_t ruid, uid_t euid, uid_t suid)
{
	/* check permissions */
	if (!suser()) {
		if ((ruid != (uid_t) -1) && (ruid != current_task->uid) && (ruid != current_task->euid) && (ruid != current_task->suid))
			return -EPERM;
		if ((euid != (uid_t) -1) && (euid != current_task->uid) && (euid != current_task->euid) && (euid != current_task->suid))
			return -EPERM;
		if ((suid != (uid_t) -1) && (suid != current_task->uid) && (suid != current_task->euid) && (suid != current_task->suid))
			return -EPERM;
	}

	if (ruid != (uid_t) -1)
		current_task->uid = ruid;
	if (euid != (uid_t) -1)
		current_task->euid = euid;

	current_task->fsuid = current_task->euid;
	if (suid != (uid_t) -1)
		current_task->suid = suid;

	return 0;
}

/*
 * Get groups system call.
 */
int sys_getgroups(int size, gid_t *list)
{
	if (size < 0)
		return -EINVAL;

	if (size) {
		if (current_task->ngroups > (size_t) size)
			return -EINVAL;

		memcpy(list, current_task->groups, sizeof(gid_t) * current_task->ngroups);
	}

	return current_task->ngroups;
}

/*
 * Set groups system call.
 */
int sys_setgroups(size_t size, const gid_t *list)
{
	size_t i;

	/* check permissions */
	if (!suser())
		return -EPERM;

	/* check number of groups */
	if (size >= NGROUPS)
		return -EPERM;

	/* set groups */
	current_task->ngroups = size;
	for (i = 0; i < size; i++)
		current_task->groups[i] = list[i];

	return 0;
}
