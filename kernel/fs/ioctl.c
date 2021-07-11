#include <fs/fs.h>
#include <proc/sched.h>
#include <stderr.h>

/*
 * Ioctl system call.
 */
int do_ioctl(int fd, int request, unsigned long arg)
{
  struct file_t *filp;

  /* check input args */
  if (fd >= NR_OPEN || fd < 0 || !current_task->filp[fd])
    return -EBADF;

  /* get current file */
  filp = current_task->filp[fd];

  /* ioctl not implemented */
  if (!filp->f_op || !filp->f_op->ioctl)
    return -EPERM;

  return filp->f_op->ioctl(filp, request, arg);
}
