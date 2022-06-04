#include <sys/syscall.h>

/*
 * Get effective group id system call.
 */
gid_t sys_getegid()
{
	return current_task->egid;
}
