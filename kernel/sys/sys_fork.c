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
	return do_fork(0, 0);
}
