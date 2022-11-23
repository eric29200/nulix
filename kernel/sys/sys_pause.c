#include <sys/syscall.h>
#include <proc/sched.h>
#include <stderr.h>

/*
 * Pause system call.
 */
int sys_pause()
{
	/* set current state sleeping and reschedule */
	current_task->state = TASK_SLEEPING;
	schedule();

	return -ERESTARTNOHAND;
}
