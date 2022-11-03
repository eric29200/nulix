#include <sys/syscall.h>

/*
 * Set process group id system call.
 */
int sys_setpgid(pid_t pid, pid_t pgid)
{
	struct task_t *task;

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
