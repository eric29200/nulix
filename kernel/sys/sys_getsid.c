#include <sys/syscall.h>
#include <stderr.h>

/*
 * Get session id system call.
 */
int sys_getsid(pid_t pid)
{
	struct task_t *task;

	/* return current session */
	if (!pid)
		return current_task->session;

	/* return matching process session */
	task = find_task(pid);
	if (task)
		return task->session;

	return -ESRCH;
}
