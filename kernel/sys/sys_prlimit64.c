#include <sys/syscall.h>
#include <stdio.h>
#include <stderr.h>

/*
 * Prlimit system call.
 */
int sys_prlimit64(pid_t pid, int resource, struct rlimit64_t *new_limit, struct rlimit64_t *old_limit)
{
	struct task_t *task;

	/* check resource */
	if (resource >= RLIM_NLIMITS)
		return -EINVAL;

	/* get task */
	if (pid)
		task = find_task(pid);
	else
		task = current_task;

	/* no matching task */
	if (!task)
		return -ESRCH;

	/* write prlimit not implemented */
	if (new_limit)
		printf("write prlimit not implemented\n");

	/* get limit */
	if (old_limit) {
		memset(old_limit, 0, sizeof(struct rlimit64_t));
		old_limit->rlim_cur = task->rlim[resource].rlim_cur;
		old_limit->rlim_max = task->rlim[resource].rlim_max;
	}

	return 0;
}
