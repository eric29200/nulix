#include <fs/fs.h>
#include <proc/sched.h>
#include <stderr.h>
#include <fcntl.h>

/*
 * Truncate system call.
 */
int do_truncate(const char *pathname, off_t length)
{
  struct inode_t *inode;

  /* get inode */
  inode = namei(AT_FDCWD, pathname, 1);
  if (!inode)
    return -ENOENT;

  /* set new size */
  inode->i_size = length;

  /* truncate */
  if (inode->i_op && inode->i_op->truncate)
    inode->i_op->truncate(inode);

  /* release inode */
  inode->i_dirt = 1;
  iput(inode);

  return 0;
}

/*
 * File truncate system call.
 */
int do_ftruncate(int fd, off_t length)
{
  struct inode_t *inode;

  /* check file descriptor */
  if (fd >= NR_OPEN || fd < 0 || !current_task->filp[fd])
    return -EBADF;

  /* get inode */
  inode = current_task->filp[fd]->f_inode;

  /* set new size */
  inode->i_size = length;

  /* truncate */
  if (inode->i_op && inode->i_op->truncate)
    inode->i_op->truncate(inode);

  /* mark inode dirty */
  inode->i_dirt = 1;

  return 0;
}
