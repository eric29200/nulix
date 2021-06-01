#include <fs/fs.h>
#include <proc/sched.h>
#include <mm/mm.h>
#include <stdio.h>
#include <stderr.h>
#include <fcntl.h>
#include <string.h>

/* global inode table */
static struct inode_t inode_table[NR_INODE];

/*
 * Get an empty inode.
 */
struct inode_t *get_empty_inode()
{
  struct inode_t *inode;
  int i;

  /* find a free inode */
  for (i = 0; i < NR_INODE; i++) {
    if (!inode_table[i].i_ref) {
      inode = &inode_table[i];
      break;
    }
  }

  /* no more inode */
  if (!inode)
    return NULL;

  /* reset inode */
  memset(inode, 0, sizeof(struct inode_t));
  inode->i_ref = 1;

  return inode;
}

/*
 * Get a pipe inode.
 */
struct inode_t *get_pipe_inode()
{
  struct inode_t *inode;

  /* get an empty inode */
  inode = get_empty_inode();
  if (!inode)
    return NULL;

  /* allocate some memory for data */
  inode->i_size = (uint32_t) kmalloc(PAGE_SIZE);
  if (!inode->i_size) {
    inode->i_ref = 0;
    return NULL;
  }

  /* set pipe inode (2 references = reader + writer) */
  inode->i_ref = 2;
  inode->i_pipe = 1;
  PIPE_RPOS(inode) = 0;
  PIPE_WPOS(inode) = 0;

  return inode;
}

/*
 * Read an inode.
 */
static struct inode_t *read_inode(struct inode_t *inode)
{
  struct minix_inode_t *minix_inode;
  struct buffer_head_t *bh;
  uint32_t block, i, j;

  if (!inode)
    return NULL;

  /* read minix inode block */
  block = 2 + inode->i_sb->s_imap_blocks + inode->i_sb->s_zmap_blocks + (inode->i_ino - 1) / MINIX_INODES_PER_BLOCK;
  bh = bread(inode->i_sb->s_dev, block);
  if (!bh) {
    iput(inode);
    return NULL;
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

  /* free minix inode */
  brelse(bh);

  return inode;
}

/*
 * Write an inode on disk.
 */
static int write_inode(struct inode_t *inode)
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
 * Get block number.
 */
int bmap(struct inode_t *inode, int block, int create)
{
  struct buffer_head_t *bh;
  int i;

  if (block < 0 || block >= 7 + 512 + 512 * 512)
    return 0;

  /* direct blocks */
  if (block < 7) {
    if (create && !inode->i_zone[block])
      if ((inode->i_zone[block] = new_block(inode->i_sb)))
        inode->i_dirt = 1;

    return inode->i_zone[block];
  }

  /* first indirect block (contains address to 512 blocks) */
  block -= 7;
  if (block < 512) {
    /* create block if needed */
    if (create && !inode->i_zone[7])
      if ((inode->i_zone[7] = new_block(inode->i_sb)))
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
      if ((i = new_block(inode->i_sb))) {
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
    if ((inode->i_zone[8] = new_block(inode->i_sb)))
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
    if ((i = new_block(inode->i_sb))) {
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
    if ((i = new_block(inode->i_sb))) {
      ((uint16_t *) (bh->b_data))[block & 511] = i;
      bh->b_dirt = 1;
    }
  }
  brelse(bh);

  return i;
}

/*
 * Get an inode.
 */
struct inode_t *iget(struct super_block_t *sb, ino_t ino)
{
  struct inode_t *inode;
  int i;

  /* try to find inode in table */
  for (i = 0; i < NR_INODE; i++) {
    if (inode_table[i].i_ino == ino) {
      inode = &inode_table[i];
      inode->i_ref++;
      return inode;
    }
  }

  /* get an empty inode */
  inode = get_empty_inode();
  if (!inode)
    return NULL;

  /* read inode */
  inode->i_dev = sb->s_dev;
  inode->i_ino = ino;
  inode->i_sb = sb;
  inode = read_inode(inode);

  return inode;
}

/*
 * Release an inode.
 */
void iput(struct inode_t *inode)
{
  if (!inode)
    return;

  /* update inode reference count */
  inode->i_ref--;

  /* special case : pipe inode */
  if (inode->i_pipe) {
    /* wakeup eventual readers/writers */
    task_wakeup(&inode->i_rwait);
    task_wakeup(&inode->i_wwait);

    /* no references : free inode */
    if (!inode->i_ref) {
      kfree((void *) inode->i_size);
      memset(inode, 0, sizeof(struct inode_t));
    }

    return;
  }

  /* removed inode : truncate and free it */
  if (!inode->i_nlinks && inode->i_ref == 0) {
    truncate(inode);
    free_inode(inode);
    return;
  }

  /* write inode if needed */
  if (inode->i_dirt) {
    write_inode(inode);
    inode->i_dirt = 0;
  }

  /* no more references : reset inode */
  if (inode->i_ref == 0)
    memset(inode, 0, sizeof(struct inode_t));
}
