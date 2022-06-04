#include <sys/syscall.h>

/*
 * Get effective user id system call.
 */
uid_t sys_geteuid()
{
	return current_task->euid;
}
