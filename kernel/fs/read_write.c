#include <fs/fs.h>
#include <fs/buffer.h>
#include <proc/sched.h>
#include <mm/mm.h>
#include <stdio.h>
#include <stderr.h>
#include <stat.h>
#include <string.h>

/*
 * Read system call.
 */
int do_read(int fd, char *buf, int count)
{
  struct file_t *filp;

  /* check input args */
  if (fd >= NR_OPEN || fd < 0 || count < 0 || !current_task->filp[fd])
    return -EINVAL;

  /* no data to read */
  if (!count)
    return 0;

  /* get current file */
  filp = current_task->filp[fd];

  /* adjust size */
  if (filp->f_pos + count > filp->f_inode->i_size)
    count = filp->f_inode->i_size - filp->f_pos;
  if (count <= 0)
    return 0;

  /* read file */
  return file_read(filp->f_inode, filp, buf, count);
}

/*
 * Write system call.
 */
int do_write(int fd, const char *buf, int count)
{
  struct file_t *filp;

  /* check input args */
  if (fd >= NR_OPEN || fd < 0 || count < 0 || !current_task->filp[fd])
    return -EINVAL;

  /* no data to write */
  if (!count)
    return 0;

  /* get current file */
  filp = current_task->filp[fd];

  /* write to character device */
  if (S_ISCHR(filp->f_inode->i_mode))
    return write_char(filp->f_inode->i_zone[0], buf, count);

  return -EINVAL;
}
