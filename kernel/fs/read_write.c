#include <fs/fs.h>
#include <proc/sched.h>
#include <mm/mm.h>
#include <stdio.h>
#include <fcntl.h>
#include <stderr.h>
#include <string.h>

/*
 * Read system call.
 */
int do_read(int fd, char *buf, int count)
{
  struct file_t *filp;

  /* check input args */
  if (fd >= NR_OPEN || fd < 0 || count < 0 || !current_task->filp[fd])
    return -EBADF;

  /* no data to read */
  if (!count)
    return 0;

  /* get current file */
  filp = current_task->filp[fd];

  /* read from character device */
  if (S_ISCHR(filp->f_inode->i_mode))
    return read_char(filp->f_inode->i_zone[0], buf, count);

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
    return -EBADF;

  /* no data to write */
  if (!count)
    return 0;

  /* get current file */
  filp = current_task->filp[fd];

  /* write to character device */
  if (S_ISCHR(filp->f_inode->i_mode))
    return write_char(filp->f_inode->i_zone[0], buf, count);

  return file_write(filp->f_inode, filp, buf, count);
}

/*
 * Lseek system call.
 */
off_t do_lseek(int fd, off_t offset, int whence)
{
  struct file_t *filp;
  off_t new_offset;

  /* check fd */
  if (fd >= NR_OPEN || fd < 0 || !current_task->filp[fd])
    return -EBADF;

  /* compute new offset */
  filp = current_task->filp[fd];
  switch (whence) {
    case SEEK_SET:
      new_offset = offset;
      break;
    case SEEK_CUR:
      new_offset = filp->f_pos + offset;
      break;
    case SEEK_END:
      new_offset = filp->f_inode->i_size + offset;
      break;
    default:
      new_offset = -1;
      break;
  }

  /* bad offset */
  if (new_offset < 0)
    return -EINVAL;

  /* change offset */
  filp->f_pos = new_offset;
  return filp->f_pos;
}
