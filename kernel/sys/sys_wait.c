#include <sys/syscall.h>
#include <proc/sched.h>
#include <proc/wait.h>
#include <stderr.h>

/*
 * Wait system call.
 */
pid_t sys_waitpid(pid_t pid, int *wstatus, int options)
{
	struct list_head_t *pos;
	struct task_t *task;
	int has_children;
	pid_t child_pid;

	/* look for a terminated child */
	for (;;) {
		has_children = 0;

		/* process interruption */
		if (!sigisemptyset(&current_task->sigpend))
			return -ERESTARTSYS;

		/* search zombie child */
		list_for_each(pos, &current_task->list) {
			task = list_entry(pos, struct task_t, list);

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
		task_sleep(&current_task->wait_child_exit);
	}
}
