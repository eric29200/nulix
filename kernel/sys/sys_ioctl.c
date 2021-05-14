#include <sys/syscall.h>
#include <drivers/tty.h>
#include <fcntl.h>
#include <stderr.h>
#include <dev.h>

/*
 * Ioctl system call.
 */
int sys_ioctl(int fd, unsigned long request, unsigned long arg)
{
  struct file_t *filp;
  dev_t dev;

  /* check file descriptor */
  if (fd >= NR_OPEN || !current_task->filp[fd])
    return -EBADF;

  /* ioctl on char/block devices only */
  filp = current_task->filp[fd];
  if (!S_ISCHR(filp->f_inode->i_mode) && !S_ISBLK(filp->f_inode->i_mode))
    return -EINVAL;

  /* actually on tty only */
  dev = filp->f_inode->i_zone[0];
  if (major(dev) == major(DEV_TTY1))
    return tty_ioctl(dev, request, arg);

  return -EINVAL;
}
