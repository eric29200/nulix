#include <sys/syscall.h>

/*
 * Rename at system call.
 */
int sys_renameat2(int olddirfd, const char *oldpath, int newdirfd, const char *newpath, unsigned int flags)
{
	return do_rename(olddirfd, oldpath, newdirfd, newpath, flags);
}
