#include <sys/syscall.h>
#include <proc/sched.h>
#include <proc/task.h>
#include <stddef.h>
#include <stderr.h>

/*
 * Fork system call.
 */
pid_t sys_fork()
{
	struct task_t *child;

	/* create child */
	child = fork_task(current_task);
	if (!child)
		return -ENOMEM;

	/* run child */
	list_add(&child->list, &current_task->list);

	/* return child pid */
	return child->pid;
}
