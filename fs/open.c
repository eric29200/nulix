#include <fs/fs.h>
#include <fs/buffer.h>
#include <proc/sched.h>
#include <mm/mm.h>
#include <stdio.h>
#include <stderr.h>
#include <stat.h>
#include <string.h>

/*
 * Open system call.
 */
int sys_open(const char *pathname)
{
  struct inode_t *inode;
  struct file_t *file;
  int fd, ret;

  /* find a free slot in current process */
  for (fd = 0; fd < NR_OPEN; fd++)
    if (!current_task->filp[fd])
      break;

  /* no slots : exit */
  if (fd >= NR_OPEN)
    return -EINVAL;

  /* open file */
  ret = open_namei(pathname, &inode);
  if (ret != 0)
    return ret;

  /* allocate a new file */
  file = (struct file_t *) kmalloc(sizeof(struct file_t));
  if (!file) {
    kfree(inode);
    return -ENOMEM;
  }

  /* set file */
  file->f_mode = inode->i_mode;
  file->f_inode = inode;
  file->f_pos = 0;
  current_task->filp[fd] = file;

  return fd;
}

/*
 * Close system call.
 */
int sys_close(int fd)
{
  struct file_t *filp;

  if (fd < 0 || fd >= NR_OPEN)
    return -EINVAL;

  filp = current_task->filp[fd];
  if (!filp)
    return -ENOMEM;

  current_task->filp[fd] = NULL;
  if (filp->f_inode)
    kfree(filp->f_inode);
  kfree(filp);

  return 0;
}

/*
 * Read a file.
 */
static int file_read(struct inode_t *inode, struct file_t *filp, char *buf, int count)
{
  int pos, nb_chars, left;
  char *block;

  left = count;
  while (left > 0) {
    /* read block */
    block = bread(inode->i_dev, inode->i_zone[filp->f_pos / BLOCK_SIZE]);

    /* find position and number of chars to read */
    pos = filp->f_pos % BLOCK_SIZE;
    nb_chars = BLOCK_SIZE - pos <= left ? BLOCK_SIZE - pos : left;

    /* copy into buffer */
    memcpy(buf, block + pos, nb_chars);

    /* free block */
    kfree(block);

    /* update sizes */
    filp->f_pos += nb_chars;
    buf += nb_chars;
    left -= nb_chars;
  }

  return count - left;
}
