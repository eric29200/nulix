#include <sys/syscall.h>
#include <stderr.h>

/*
 * Kill system call (send a signal to a process).
 */
int sys_kill(pid_t pid, int sig)
{
	struct task_t *task;

	/* check signal number (0 is ok : means check permission but do not send signal) */
	if (sig < 0 || sig >= NSIGS)
		return -EINVAL;

	/* send signal to process identified by pid */
	if (pid > 0)
		return task_signal(pid, sig);

	/* send signal to all processes in the group of given pid */
	if (pid == 0) {
		task = get_task(pid);
		if (!task)
			return -EINVAL;

		return task_signal_group(task->pgrp, sig);
	}

	/* send signal to all processes (except init) */
	if (pid == -1)
		task_signal_all(sig);

	/* signal to all processes in the group -pid */
	return task_signal_group(-pid, sig);
}
