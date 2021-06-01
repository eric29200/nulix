#include <fs/minix_fs.h>
#include <drivers/tty.h>
#include <proc/sched.h>
#include <stderr.h>
#include <dev.h>

/*
 * Open a character device.
 */
int minix_char_open(struct file_t *filp)
{
  dev_t dev = filp->f_inode->i_zone[0];

  /* get a new tty */
  if (dev == DEV_TTY) {
    current_task->tty = tty_get();
    if ((int) current_task->tty < 0)
      return -EBUSY;
  }

  return 0;
}

/*
 * Read from a character device.
 */
int minix_char_read(struct file_t *filp, char *buf, int count)
{
  dev_t dev = filp->f_inode->i_zone[0];

  /* TTY devices */
  if (major(dev) == major(DEV_TTY) || major(dev) == major(DEV_TTY0))
    return tty_read(dev, buf, count);

  return -EINVAL;
}

/*
 * Write to a character device.
 */
int minix_char_write(struct file_t *filp, const char *buf, int count)
{
  dev_t dev = filp->f_inode->i_zone[0];

  /* TTY devices */
  if (major(dev) == major(DEV_TTY) || major(dev) == major(DEV_TTY0))
    return tty_write(dev, buf, count);

  return -EINVAL;
}

