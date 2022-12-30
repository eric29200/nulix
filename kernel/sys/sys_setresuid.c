#include <sys/syscall.h>

/*
 * Set uid system call.
 */
int sys_setresuid(uid_t ruid, uid_t euid, uid_t suid)
{
	UNUSED(ruid);
	UNUSED(euid);
	UNUSED(suid);
	return 0;
}