#include <sys/syscall.h>
#include <stderr.h>

/*
 * Clone system call.
 */
int sys_clone(uint32_t flags, uint32_t newsp)
{
	struct task_t *child;

	/* unused dlags */
	UNUSED(flags);

	/* create child */
	child = fork_task(current_task, newsp);
	if (!child)
		return -ENOMEM;

	/* run child */
	list_add(&child->list, &current_task->list);

	/* return child pid */
	return child->pid;
}
