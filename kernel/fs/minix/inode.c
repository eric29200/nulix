#include <fs/fs.h>
#include <proc/sched.h>
#include <mm/mm.h>
#include <stdio.h>
#include <stderr.h>
#include <fcntl.h>
#include <string.h>

/*
 * Minix inode operations.
 */
static struct inode_operations_t minix_inode_operations = {
  .lookup         = minix_lookup,
  .link           = minix_link,
  .unlink         = minix_unlink,
  .symlink        = minix_symlink,
  .mkdir          = minix_mkdir,
  .rmdir          = minix_rmdir,
};

/*
 * Read an inode.
 */
int minix_read_inode(struct inode_t *inode)
{
  struct minix_inode_t *minix_inode;
  struct buffer_head_t *bh;
  uint32_t block, i, j;

  if (!inode)
    return -EINVAL;

  /* read minix inode block */
  block = 2 + inode->i_sb->s_imap_blocks + inode->i_sb->s_zmap_blocks + (inode->i_ino - 1) / MINIX_INODES_PER_BLOCK;
  bh = bread(inode->i_sb->s_dev, block);
  if (!bh) {
    iput(inode);
    return -EIO;
  }

  /* read minix inode */
  minix_inode = (struct minix_inode_t *) bh->b_data;
  i = (inode->i_ino - 1) % MINIX_INODES_PER_BLOCK;

  /* fill in memory inode */
  inode->i_mode = minix_inode[i].i_mode;
  inode->i_uid = minix_inode[i].i_uid;
  inode->i_size = minix_inode[i].i_size;
  inode->i_time = minix_inode[i].i_time;
  inode->i_gid = minix_inode[i].i_gid;
  inode->i_nlinks = minix_inode[i].i_nlinks;
  for (j = 0; j < 9; j++)
    inode->i_zone[j] = minix_inode[i].i_zone[j];
  inode->i_op = &minix_inode_operations;

  /* free minix inode */
  brelse(bh);

  return 0;
}

/*
 * Write an inode on disk.
 */
int minix_write_inode(struct inode_t *inode)
{
  struct minix_inode_t *minix_inode;
  struct buffer_head_t *bh;
  uint32_t block, i, j;

  if (!inode)
    return -EINVAL;

  /* read minix inode block */
  block = 2 + inode->i_sb->s_imap_blocks + inode->i_sb->s_zmap_blocks + (inode->i_ino - 1) / MINIX_INODES_PER_BLOCK;
  bh = bread(inode->i_sb->s_dev, block);
  if (!bh)
    return -EIO;

  /* read minix inode */
  minix_inode = (struct minix_inode_t *) bh->b_data;
  i = (inode->i_ino - 1) % MINIX_INODES_PER_BLOCK;

  /* fill in on disk inode */
  minix_inode[i].i_mode = inode->i_mode;
  minix_inode[i].i_uid = inode->i_uid;
  minix_inode[i].i_size = inode->i_size;
  minix_inode[i].i_time = inode->i_time;
  minix_inode[i].i_gid = inode->i_gid;
  minix_inode[i].i_nlinks = inode->i_nlinks;
  for (j = 0; j < 9; j++)
    minix_inode[i].i_zone[j] = inode->i_zone[j];

  /* write inode block */
  bh->b_dirt = 1;
  brelse(bh);

  return 0;
}

/*
 * Put an inode.
 */
int minix_put_inode(struct inode_t *inode)
{
  /* check inode */
  if (!inode)
    return -EINVAL;

  /* truncate and free inode */
  if (!inode->i_nlinks) {
    minix_truncate(inode);
    free_inode(inode);
  }

  return 0;
}
