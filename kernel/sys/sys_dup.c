#include <sys/syscall.h>
#include <proc/sched.h>
#include <fs/fs.h>
#include <stderr.h>

/*
 * Dup system call.
 */
int sys_dup(int oldfd)
{
  return do_dup(oldfd);
}
