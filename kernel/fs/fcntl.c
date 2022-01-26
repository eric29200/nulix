#include <fs/fs.h>
#include <proc/sched.h>
#include <stdio.h>
#include <stderr.h>
#include <fcntl.h>

/*
 * Dup system call.
 */
static int dup_after(struct file_t *filp, int min_slot)
{
  int newfd;

  /* find a free slot */
  for (newfd = min_slot; newfd < NR_OPEN; newfd++) {
    if (current_task->filp[newfd] == NULL) {
      current_task->filp[newfd] = filp;
      filp->f_ref++;
      return newfd;
    }
  }

  /* no free slot : too many files open */
  return -EMFILE;
}

/*
 * Fcntl system call.
 */
int do_fcntl(int fd, int cmd, unsigned long arg)
{
  struct file_t *filp;
  int ret = 0;

  /* check fd */
  if (fd >= NR_OPEN || !current_task->filp[fd])
    return -EINVAL;

  filp = current_task->filp[fd];
  switch (cmd) {
      case F_DUPFD:
        ret = dup_after(filp, arg);
        break;
      case F_GETFD:
        ret = filp->f_flags;
        break;
      case F_SETFD:
        filp->f_flags = arg;
        break;
      case F_GETFL:
        ret = filp->f_mode;
        break;
      case F_SETFL:
        filp->f_mode = ret;
        break;
      default:
        printf("unknown fcntl command %d\n", cmd);
        break;
  }

  return ret;
}
