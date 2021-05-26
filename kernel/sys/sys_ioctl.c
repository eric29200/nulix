#include <sys/syscall.h>
#include <drivers/tty.h>
#include <fcntl.h>
#include <stderr.h>
#include <stdio.h>
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

  /* ioctl on tty only */
  filp = current_task->filp[fd];
  if (S_ISCHR(filp->f_inode->i_mode)) {
    dev = filp->f_inode->i_zone[0];

    if (major(dev) == major(DEV_TTY) || major(dev) == major(DEV_TTY0))
      return tty_ioctl(dev, request, arg);
  }

  printf("Unknown ioctl (request=%d) on file (fd=%d)\n", request, fd);
  return -EINVAL;
}
