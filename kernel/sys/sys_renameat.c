#include <sys/syscall.h>

/*
 * Rename at system call.
 */
int sys_renameat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath)
{
	return do_rename(olddirfd, oldpath, newdirfd, newpath);
}
