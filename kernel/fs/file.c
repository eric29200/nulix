#include <fs/fs.h>
#include <proc/sched.h>
#include <mm/mm.h>
#include <stdio.h>
#include <stderr.h>
#include <fcntl.h>
#include <string.h>

/*
 * Read a file.
 */
int file_read(struct file_t *filp, char *buf, int count)
{
  int pos, nb_chars, left, block_nr;
  struct buffer_head_t *bh;

  /* adjust size */
  if (filp->f_pos + count > filp->f_inode->i_size)
    count = filp->f_inode->i_size - filp->f_pos;

  if (count <= 0)
    return 0;

  left = count;
  while (left > 0) {
    /* get block number */
    block_nr = bmap(filp->f_inode, filp->f_pos / BLOCK_SIZE, 0);
    if (!block_nr)
      goto out;

    /* read block */
    bh = bread(filp->f_inode->i_dev, block_nr);
    if (!bh)
      goto out;

    /* find position and number of chars to read */
    pos = filp->f_pos % BLOCK_SIZE;
    nb_chars = BLOCK_SIZE - pos <= left ? BLOCK_SIZE - pos : left;

    /* copy into buffer */
    memcpy(buf, bh->b_data + pos, nb_chars);

    /* release block */
    brelse(bh);

    /* update sizes */
    filp->f_pos += nb_chars;
    buf += nb_chars;
    left -= nb_chars;
  }

out:
  return count - left;
}

/*
 * Write to a file.
 */
int file_write(struct file_t *filp, const char *buf, int count)
{
  uint32_t pos, nb_chars, left;
  struct buffer_head_t *bh;
  int block_nr;

  /* handle append flag */
  if (filp->f_flags & O_APPEND)
    pos = filp->f_inode->i_size;
  else
    pos = filp->f_pos;

  left = count;
  while (left > 0) {
    /* get/create block number */
    block_nr = bmap(filp->f_inode, pos / BLOCK_SIZE, 1);
    if (!block_nr)
      goto out;

    /* read block */
    bh = bread(filp->f_inode->i_dev, block_nr);
    if (!bh)
      goto out;

    /* find position and number of chars to read */
    pos = pos % BLOCK_SIZE;
    nb_chars = BLOCK_SIZE - pos <= left ? BLOCK_SIZE - pos : left;

    /* copy into buffer */
    memcpy(bh->b_data + pos, buf, nb_chars);

    /* release block */
    bh->b_dirt = 1;
    brelse(bh);

    /* update sizes */
    pos += nb_chars;
    buf += nb_chars;
    left -= nb_chars;

    /* end of file : grow it and mark inode dirty */
    if (pos > filp->f_inode->i_size) {
      filp->f_inode->i_size = pos;
      filp->f_inode->i_dirt = 1;
    }
  }

out:
  filp->f_pos = pos;
  return count - left;
}
