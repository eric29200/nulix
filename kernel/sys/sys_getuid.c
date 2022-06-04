#include <sys/syscall.h>

/*
 * Get user id system call.
 */
uid_t sys_getuid()
{
	return current_task->uid;
}
