#include <sys/syscall.h>

/*
 * Set gid system call.
 */
int sys_setresgid(gid_t rgid, gid_t egid, gid_t sgid)
{
	UNUSED(rgid);
	UNUSED(egid);
	UNUSED(sgid);
	return 0;
}