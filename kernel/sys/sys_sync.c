#include <sys/syscall.h>

/*
 * Sync system call.
 */
int sys_sync()
{
	/* sync all buffers */
	bsync();

	return 0;
}
