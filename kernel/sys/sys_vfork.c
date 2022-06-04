#include <sys/syscall.h>

/*
 * Vfork system call..
 */
pid_t sys_vfork()
{
	return sys_fork();
}
