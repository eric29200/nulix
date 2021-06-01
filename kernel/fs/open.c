#include <fs/fs.h>
#include <mm/mm.h>
#include <drivers/tty.h>
#include <proc/sched.h>
#include <stdio.h>
#include <stderr.h>
#include <fcntl.h>
#include <string.h>

/* global file table */
struct file_t filp_table[NR_FILE];

/*
 * Open system call.
 */
int do_open(int dirfd, const char *pathname, int flags, mode_t mode)
{
  struct inode_t *inode;
  struct file_t *filp;
  int fd, i, ret;

  /* find a free slot in current process */
  for (fd = 0; fd < NR_OPEN; fd++)
    if (!current_task->filp[fd])
      break;

  /* no slots : exit */
  if (fd >= NR_OPEN)
    return -EINVAL;

  /* find a free file descriptor in global table */
  for (i = 0; i < NR_FILE; i++)
    if (!filp_table[i].f_ref)
      break;

  /* no free file in global table */
  if (i >= NR_FILE)
    return -EINVAL;

  /* open file */
  ret = open_namei(dirfd, pathname, flags, mode, &inode);
  if (ret != 0)
    return ret;

  /* set file */
  filp = current_task->filp[fd] = &filp_table[i];
  filp->f_mode = inode->i_mode;
  filp->f_inode = inode;
  filp->f_flags = flags;
  filp->f_pos = 0;
  filp->f_ref++;
  filp->f_op = inode->i_op->fops;

  /* specific open function */
  if (filp->f_op && filp->f_op->open)
    filp->f_op->open(filp);

  return fd;
}

/*
 * Close system call.
 */
int do_close(int fd)
{
  if (fd < 0 || fd >= NR_OPEN || !current_task->filp[fd])
    return -EINVAL;

  /* release file if not used anymore */
  current_task->filp[fd]->f_ref--;
  if (current_task->filp[fd]->f_ref <= 0) {
    iput(current_task->filp[fd]->f_inode);
    memset(current_task->filp[fd], 0, sizeof(struct file_t));
  }

  current_task->filp[fd] = NULL;

  return 0;
}
