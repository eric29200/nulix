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
 * Get an empty file.
 */
struct file_t *get_empty_filp()
{
  int i;

  for (i = 0; i < NR_FILE; i++)
    if (!filp_table[i].f_ref)
      break;

  if (i >= NR_FILE)
    return NULL;

  filp_table[i].f_ref = 1;
  return &filp_table[i];
}

/*
 * Open system call.
 */
int do_open(int dirfd, const char *pathname, int flags, mode_t mode)
{
  struct inode_t *inode;
  struct file_t *filp;
  int fd, ret;

  /* find a free slot in current process */
  for (fd = 0; fd < NR_OPEN; fd++)
    if (!current_task->filp[fd])
      break;

  /* no slots : exit */
  if (fd >= NR_OPEN)
    return -EINVAL;

  /* get an empty file */
  filp = get_empty_filp();
  if (!filp)
    return -EINVAL;

  /* open file */
  ret = open_namei(dirfd, pathname, flags, mode, &inode);
  if (ret != 0)
    return ret;

  /* set file */
  current_task->filp[fd] = filp;
  filp->f_mode = inode->i_mode;
  filp->f_inode = inode;
  filp->f_flags = flags;
  filp->f_pos = 0;
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
  /* check file descriptor */
  if (fd < 0 || fd >= NR_OPEN || !current_task->filp[fd])
    return -EINVAL;

  /* release file if not used anymore */
  current_task->filp[fd]->f_ref--;
  if (current_task->filp[fd]->f_ref <= 0) {
    /* specific close operation */
    if (current_task->filp[fd]->f_op && current_task->filp[fd]->f_op->close)
      current_task->filp[fd]->f_op->close(current_task->filp[fd]);

    /* release inode */
    iput(current_task->filp[fd]->f_inode);
    memset(current_task->filp[fd], 0, sizeof(struct file_t));
  }

  current_task->filp[fd] = NULL;

  return 0;
}

/*
 * Chmod system call.
 */
int do_chmod(const char *filename, mode_t mode)
{
  struct inode_t *inode;

  /* get inode */
  inode = namei(AT_FDCWD, filename, 1);
  if (!inode)
    return -ENOSPC;

  /* adjust mode */
  if (mode == (mode_t) - 1)
    mode = inode->i_mode;

  /* change mode */
  inode->i_mode = (mode & S_IALLUGO) | (inode->i_mode & ~S_IALLUGO);
  inode->i_dirt = 1;
  iput(inode);

  return 0;
}

/*
 * Chown system call.
 */
int do_chown(const char *pathname, uid_t owner, gid_t group)
{
  struct inode_t *inode;

  /* get inode */
  inode = namei(AT_FDCWD, pathname, 1);
  if (!inode)
    return -ENOSPC;

  /* update inode */
  inode->i_uid = owner;
  inode->i_gid = group;
  inode->i_dirt = 1;
  iput(inode);

  return 0;
}

/*
 * Fchown system call.
 */
int do_fchown(int fd, uid_t owner, gid_t group)
{
  struct inode_t *inode;

  /* check file descriptor */
  if (fd < 0 || fd >= NR_OPEN || !current_task->filp[fd])
    return -EINVAL;


  /* update inode */
  inode = current_task->filp[fd]->f_inode;
  inode->i_uid = owner;
  inode->i_gid = group;
  inode->i_dirt = 1;
  iput(inode);

  return 0;
}
