#include <sys/syscall.h>
#include <proc/sched.h>
#include <fs/fs.h>
#include <stderr.h>

/*
 * Dup2 system call.
 */
int sys_dup2(int oldfd, int newfd)
{
  int ret;

  /* check parameters */
  if (oldfd < 0 || oldfd >= NR_OPEN || !current_task->filp[oldfd] || newfd < 0 || newfd >= NR_OPEN)
    return -EBADF;

  /* same fd */
  if (oldfd == newfd)
    return oldfd;

  /* close existing file */
  if (current_task->filp[newfd] != NULL) {
    ret = sys_close(newfd);
    if (ret < 0)
      return ret;
  }

  /* duplicate */
  current_task->filp[newfd] = current_task->filp[oldfd];
  current_task->filp[newfd]->f_ref++;

  return newfd;
}
