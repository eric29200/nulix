#include <sys/syscall.h>

/*
 * Get group id system call.
 */
gid_t sys_getgid()
{
	return current_task->gid;
}
