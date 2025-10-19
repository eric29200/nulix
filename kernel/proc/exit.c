#include <proc/sched.h>
#include <proc/ptrace.h>
#include <drivers/char/tty.h>
#include <stderr.h>

/*
 * Exit a task.
 */
void do_exit(int error_code)
{
	struct list_head *pos;
	struct task *child;

	/* delete signal timer */
	if (current_task->sig_tm.list.next)
		timer_event_del(&current_task->sig_tm);

	/* delete timeout timer */
	if (current_task->timeout_tm.list.next)
		timer_event_del(&current_task->timeout_tm);

	/* free resources */
	task_exit_signals(current_task);
	task_exit_files(current_task);
	task_exit_fs(current_task);
	task_exit_mm(current_task);

	/* mark task terminated and reschedule */
	current_task->state = TASK_ZOMBIE;
	current_task->exit_code = error_code;

	/* notify parent */
	__task_signal(current_task->parent, SIGCHLD);
	wake_up(&current_task->parent->wait_child_exit);

	/* give children to init */
	list_for_each(pos, &tasks_list) {
		child = list_entry(pos, struct task, list);
		if (child->parent == current_task) {
			child->parent = init_task;
			if (child->state == TASK_ZOMBIE)
				wake_up(&init_task->wait_child_exit);

			/* reset ptrace */
			child->ptrace &= ~(PT_PTRACED | PT_TRACESYS);
		}
	}

	/* leader process : disassociate tty */
	if (current_task->leader)
		disassociate_ctty();

	/* call scheduler */
	schedule();
}

/*
 * Exit group system call.
 */
void sys_exit_group(int status)
{
	sys_exit(status);
}

/*
 * Wait system call.
 */
pid_t sys_waitpid(pid_t pid, int *wstatus, int options)
{
	struct list_head *pos;
	struct task *task;
	int has_children;
	pid_t child_pid;

	/* look for a terminated child */
	for (;;) {
		has_children = 0;

		/* search zombie child */
		list_for_each(pos, &tasks_list) {
			task = list_entry(pos, struct task, list);

			/* check task (see man waitpid) */
			if (pid > 0 && task->pid != pid)
				continue;
			else if (pid == 0 && task->pgrp != current_task->pgrp)
				continue;
			else if (pid < -1 && task->pgrp != -pid)
				continue;
			else if (pid == -1 && task->parent != current_task)
				continue;

			has_children = 1;

			/* return first stopped task pid (mark exit code to 0 to report this task just once) */
			if (task->state == TASK_STOPPED && task->exit_code) {
				if (wstatus != NULL)
					*wstatus = (task->exit_code << 8) | 0x7F;

				task->exit_code = 0;
				return task->pid;
			}

			/* destroy first zombie task */
			if (task->state == TASK_ZOMBIE) {
				current_task->cutime += task->utime + task->cutime;
				current_task->cstime += task->stime + task->cstime;

				if (wstatus != NULL)
					*wstatus = task->exit_code;

				child_pid = task->pid;
				destroy_task(task);
				return child_pid;
			}
		}

		/* no children : return error */
		if (!has_children)
			return -ECHILD;

		/* no wait : return */
		if (options & WNOHANG)
			return 0;

		/* else wait for child */
		sleep_on(&current_task->wait_child_exit);

		/* remove SIGCHLD */
		current_task->signal &= ~(1 << SIGCHLD);

		/* process interruption */
		if (signal_pending(current_task))
			return -ERESTARTSYS;
	}
}

/*
 * Exit system call.
 */
void sys_exit(int status)
{
	return do_exit((status & 0xFF) << 8);
}

/*
 * Wait 4 system call.
 */
pid_t sys_wait4(pid_t pid, int *wstatus, int options, struct rusage *rusage)
{
	UNUSED(rusage);
	return sys_waitpid(pid, wstatus, options);
}
