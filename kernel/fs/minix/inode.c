#include <fs/minix_fs.h>
#include <proc/sched.h>
#include <mm/mm.h>
#include <stdio.h>
#include <stderr.h>
#include <fcntl.h>
#include <string.h>

/*
 * Directory operations.
 */
struct file_operations_t minix_dir_fops = {
  .read           = minix_file_read,
  .write          = minix_file_write,
  .getdents       = minix_getdents,
  .getdents64     = minix_getdents64,
};

/*
 * File operations.
 */
struct file_operations_t minix_file_fops = {
  .read           = minix_file_read,
  .write          = minix_file_write,
  .getdents       = NULL,
  .getdents64     = NULL,
};

/*
 * Char device operations.
 */
struct file_operations_t minix_char_fops = {
  .read           = minix_char_read,
  .write          = minix_char_write,
  .getdents       = NULL,
  .getdents64     = NULL,
};

/*
 * Minix file inode operations.
 */
struct inode_operations_t minix_file_iops = {
  .fops           = &minix_file_fops,
  .lookup         = minix_lookup,
  .create         = minix_create,
  .follow_link    = minix_follow_link,
  .readlink       = minix_readlink,
  .link           = minix_link,
  .unlink         = minix_unlink,
  .symlink        = minix_symlink,
  .mkdir          = minix_mkdir,
  .rmdir          = minix_rmdir,
  .truncate       = minix_truncate,
  .bmap           = minix_bmap,
};

/*
 * Minix directory inode operations.
 */
struct inode_operations_t minix_dir_iops = {
  .fops           = &minix_dir_fops,
  .lookup         = minix_lookup,
  .create         = minix_create,
  .follow_link    = minix_follow_link,
  .readlink       = minix_readlink,
  .link           = minix_link,
  .unlink         = minix_unlink,
  .symlink        = minix_symlink,
  .mkdir          = minix_mkdir,
  .rmdir          = minix_rmdir,
  .truncate       = minix_truncate,
  .bmap           = minix_bmap,
};

/*
 * Minix char device inode operations.
 */
struct inode_operations_t minix_char_iops = {
  .fops           = &minix_char_fops,
  .lookup         = minix_lookup,
  .create         = minix_create,
  .follow_link    = minix_follow_link,
  .readlink       = minix_readlink,
  .link           = minix_link,
  .unlink         = minix_unlink,
  .symlink        = minix_symlink,
  .mkdir          = minix_mkdir,
  .rmdir          = minix_rmdir,
  .truncate       = minix_truncate,
  .bmap           = minix_bmap,
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

  if (S_ISDIR(inode->i_mode))
    inode->i_op = &minix_dir_iops;
  else if (S_ISCHR(inode->i_mode))
    inode->i_op = &minix_char_iops;
  else
    inode->i_op = &minix_file_iops;

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
    minix_free_inode(inode);
  }

  return 0;
}

/*
 * Get block number.
 */
int minix_bmap(struct inode_t *inode, int block, int create)
{
  struct buffer_head_t *bh;
  int i;

  if (block < 0 || block >= 7 + 512 + 512 * 512)
    return 0;

  /* direct blocks */
  if (block < 7) {
    if (create && !inode->i_zone[block])
      if ((inode->i_zone[block] = minix_new_block(inode->i_sb)))
        inode->i_dirt = 1;

    return inode->i_zone[block];
  }

  /* first indirect block (contains address to 512 blocks) */
  block -= 7;
  if (block < 512) {
    /* create block if needed */
    if (create && !inode->i_zone[7])
      if ((inode->i_zone[7] = minix_new_block(inode->i_sb)))
        inode->i_dirt = 1;

    if (!inode->i_zone[7])
      return 0;

    /* read indirect block */
    bh = bread(inode->i_dev, inode->i_zone[7]);
    if (!bh)
      return 0;

    /* get matching block */
    i = ((uint16_t *) bh->b_data)[block];
    if (create && !i) {
      if ((i = minix_new_block(inode->i_sb))) {
        ((uint16_t *) (bh->b_data))[block] = i;
        bh->b_dirt = 1;
      }
    }
    brelse(bh);
    return i;
  }

  /* second indirect block */
  block -= 512;
  if (create && !inode->i_zone[8])
    if ((inode->i_zone[8] = minix_new_block(inode->i_sb)))
      inode->i_dirt = 1;

  if (!inode->i_zone[8])
    return 0;

  /* read second indirect block */
  bh = bread(inode->i_dev, inode->i_zone[8]);
  if (!bh)
    return 0;
  i = ((uint16_t *) bh->b_data)[block >> 9];

  /* create it if needed */
  if (create && !i) {
    if ((i = minix_new_block(inode->i_sb))) {
      ((uint16_t *) (bh->b_data))[block >> 9] = i;
      bh->b_dirt = 1;
    }
  }
  brelse(bh);

  if (!i)
    return 0;

  /* get second second indirect block */
  bh = bread(inode->i_dev, i);
  if (!bh)
    return 0;
  i = ((uint16_t *) bh->b_data)[block & 511];
  if (create && !i) {
    if ((i = minix_new_block(inode->i_sb))) {
      ((uint16_t *) (bh->b_data))[block & 511] = i;
      bh->b_dirt = 1;
    }
  }
  brelse(bh);

  return i;
}

