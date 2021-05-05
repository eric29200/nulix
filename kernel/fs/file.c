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
int file_read(struct inode_t *inode, struct file_t *filp, char *buf, int count)
{
  int pos, nb_chars, left, block_nr;
  struct buffer_head_t *bh;

  left = count;
  while (left > 0) {
    /* get block number */
    block_nr = bmap(inode, filp->f_pos / BLOCK_SIZE, 0);

    /* read block */
    bh = bread(inode->i_dev, block_nr);

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

  return count - left;
}
