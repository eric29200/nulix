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
};

/*
 * Char device operations.
 */
struct file_operations_t minix_char_fops = {
  .open           = minix_char_open,
  .read           = minix_char_read,
  .write          = minix_char_write,
};

/*
 * Minix file inode operations.
 */
struct inode_operations_t minix_file_iops = {
  .fops           = &minix_file_fops,
  .follow_link    = minix_follow_link,
  .readlink       = minix_readlink,
  .truncate       = minix_truncate,
};

/*
 * Minix directory inode operations.
 */
struct inode_operations_t minix_dir_iops = {
  .fops           = &minix_dir_fops,
  .lookup         = minix_lookup,
  .create         = minix_create,
  .link           = minix_link,
  .unlink         = minix_unlink,
  .symlink        = minix_symlink,
  .mkdir          = minix_mkdir,
  .rmdir          = minix_rmdir,
  .truncate       = minix_truncate,
};

/*
 * Minix char device inode operations.
 */
struct inode_operations_t minix_char_iops = {
  .fops           = &minix_char_fops,
  .truncate       = minix_truncate,
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

struct buffer_head_t *inode_getblk(struct inode_t *inode, int nr, int create)
{
  /* create block if needed */
  if (create && !inode->i_zone[nr])
    if ((inode->i_zone[nr] = minix_new_block(inode->i_sb)))
      inode->i_dirt = 1;

  if (!inode->i_zone[nr])
    return NULL;

  return bread(inode->i_dev, inode->i_zone[nr]);
}

struct buffer_head_t *block_getblk(struct inode_t *inode, struct buffer_head_t *bh, int block, int create)
{
  int i;

  if (!bh)
    return NULL;

  i = ((uint16_t *) bh->b_data)[block];
  if (create && !i) {
    if ((i = minix_new_block(inode->i_sb))) {
      ((uint16_t *) (bh->b_data))[block] = i;
      bh->b_dirt = 1;
    }
  }

  brelse(bh);

  if (!i)
    return NULL;

  return bread(inode->i_dev, i);
}

struct buffer_head_t *minix_bread(struct inode_t *inode, int block, int create)
{
  struct buffer_head_t *bh;

  /* check block number */
  if (block < 0 || (uint32_t) block >= inode->i_sb->s_max_size / BLOCK_SIZE)
    return NULL;

  if (block < 7)
    return inode_getblk(inode, block, create);

  block -= 7;
  if (block < 512) {
    bh = inode_getblk(inode, 7, create);
    return block_getblk(inode, bh, block, create);
  }

  block -= 512;
  bh = inode_getblk(inode, 8, create);
  bh = block_getblk(inode, bh, (block >> 9) & 511, create);
  return block_getblk(inode, bh, block & 511, create);
}
