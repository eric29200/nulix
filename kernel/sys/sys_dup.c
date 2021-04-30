#include <sys/syscall.h>
#include <proc/sched.h>
#include <fs/fs.h>
#include <stderr.h>

/*
 * Dup system call.
 */
int sys_dup(int oldfd)
{
  int newfd;

  /* check parameter */
  if (oldfd < 0 || oldfd >= NR_OPEN || current_task->filp[oldfd] == NULL)
    return -EBADF;

  /* find a free slot */
  for (newfd = 0; newfd < NR_OPEN; newfd++) {
    if (current_task->filp[newfd] == NULL) {
      current_task->filp[newfd] = current_task->filp[oldfd];
      current_task->filp[newfd]->f_ref++;
    }
  }

  /* no free slot : too many files open */
  if (newfd == NR_OPEN)
    newfd = -EMFILE;

  return newfd;
}
