#include <sys/syscall.h>

/*
 * Set user id system call.
 */
int sys_setuid(uid_t uid)
{
	current_task->uid = uid;
	return 0;
}
