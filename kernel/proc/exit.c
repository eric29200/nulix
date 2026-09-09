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

	/* delete timer */
	del_timer(&current->real_timer);

	/* free resources */
	sem_exit();
	task_exit_signals(current);
	task_exit_files(current);
	task_exit_fs(current);
	task_exit_mm(current);

	/* mark task terminated and reschedule */
	current->state = TASK_ZOMBIE;
	current->exit_code = error_code;

	/* notify parent */
	notify_parent(current, SIGCHLD);

	/* give children to init */
	list_for_each(pos, &tasks_list) {
		child = list_entry(pos, struct task, list);
		if (child->parent == current) {
			child->parent = init_task;
			if (child->state == TASK_ZOMBIE)
				wake_up(&init_task->wait_child_exit);

			/* reset ptrace */
			child->ptrace &= ~(PT_PTRACED | PT_TRACESYS);
		}
	}

	/* leader process : disassociate tty */
	if (current->leader)
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
			else if (pid == 0 && task->pgrp != current->pgrp)
				continue;
			else if (pid < -1 && task->pgrp != -pid)
				continue;
			else if (pid == -1 && task->parent != current)
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
				current->cutime += task->utime + task->cutime;
				current->cstime += task->stime + task->cstime;

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

		/* process interruption */
		if (signal_pending(current))
			return -ERESTARTSYS;

		/* else wait for child */
		sleep_on(&current->wait_child_exit);
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
