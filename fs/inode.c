#include <fs/fs.h>
#include <fs/buffer.h>
#include <proc/sched.h>
#include <mm/mm.h>
#include <stdio.h>
#include <stderr.h>
#include <stat.h>
#include <string.h>

/*
 * Read an inode.
 */
struct inode_t *read_inode(struct minix_super_block_t *sb, ino_t ino)
{
  struct minix_inode_t *minix_inode;
  struct inode_t *inode;
  uint32_t block, i, j;

  /* allocate a new inode */
  inode = (struct inode_t *) kmalloc(sizeof(struct inode_t));
  if (!inode)
    return NULL;

  /* read minix inode */
  block = 2 + sb->s_imap_blocks + sb->s_zmap_blocks + (ino - 1) / MINIX_INODES_PER_BLOCK;
  minix_inode = (struct minix_inode_t *) bread(sb->s_dev, block);
  i = (ino - 1) % MINIX_INODES_PER_BLOCK;

  /* fill in memory inode */
  inode->i_mode = minix_inode[i].i_mode;
  inode->i_uid = minix_inode[i].i_uid;
  inode->i_size = minix_inode[i].i_size;
  inode->i_time = minix_inode[i].i_time;
  inode->i_gid = minix_inode[i].i_gid;
  inode->i_nlinks = minix_inode[i].i_nlinks;
  for (j = 0; j < 9; j++)
    inode->i_zone[j] = minix_inode[i].i_zone[j];
  inode->i_ino = ino;
  inode->i_sb = sb;
  inode->i_dev = sb->s_dev;

  /* free minix inode */
  kfree(minix_inode);

  return inode;
}

/*
 * Get block number.
 */
int bmap(struct inode_t *inode, int block)
{
  uint16_t *buf;
  int i, ret;

  if (block < 0 || block >= 7 + 512 + 512 * 512)
    return -EINVAL;

  /* direct blocks */
  if (block < 7)
    return inode->i_zone[block];

  /* first indirect block (contains address to 512 blocks) */
  block -= 7;
  if (block < 512) {
    buf = (uint16_t *) bread(inode->i_dev, inode->i_zone[7]);
    ret = buf[block];
    kfree(buf);
    return ret;
  }

  /* get first second indirect block */
  block -= 512;
  buf = (uint16_t *) bread(inode->i_dev, inode->i_zone[8]);
  i = buf[block >> 9];
  kfree(buf);

  /* get second second indirect block */
  buf = (uint16_t *) bread(inode->i_dev, i);
  ret = buf[block & 511];
  kfree(buf);

  return ret;
}
