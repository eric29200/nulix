#include <sys/syscall.h>

/*
 * Signal return system call.
 */
int sys_sigreturn()
{
	return do_sigreturn();
}
