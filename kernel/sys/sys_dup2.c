#include <sys/syscall.h>
#include <proc/sched.h>
#include <fs/fs.h>
#include <stderr.h>

/*
 * Dup2 system call.
 */
int sys_dup2(int oldfd, int newfd)
{
  return do_dup2(oldfd, newfd);
}
