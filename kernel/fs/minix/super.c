#include <fs/minix_fs.h>
#include <stderr.h>
#include <stdio.h>

/*
 * Minix super operations.
 */
struct super_operations_t minix_sops = {
  .read_inode     = minix_read_inode,
  .write_inode    = minix_write_inode,
  .put_inode      = minix_put_inode,
};

/*
 * Read super block.
 */
int minix_read_super(struct super_block_t *sb, struct ata_device_t *dev)
{
  struct minix_super_block_t *msb;
  struct buffer_head_t *bh;
  uint32_t block;
  int i, ret;

  /* read super block */
  bh = bread(dev, 1);
  if (!bh)
    goto bad_sb;

  /* read minix super block */
  msb = (struct minix_super_block_t *) bh->b_data;

  /* check magic number */
  if (msb->s_magic != MINIX_SUPER_MAGIC) {
    ret = -EINVAL;
    goto err_magic;
  }

  /* set root super block */
  sb->s_ninodes = msb->s_ninodes;
  sb->s_nzones = msb->s_nzones;
  sb->s_imap_blocks = msb->s_imap_blocks;
  sb->s_zmap_blocks = msb->s_zmap_blocks;
  sb->s_firstdatazone = msb->s_firstdatazone;
  sb->s_log_zone_size = msb->s_log_zone_size;
  sb->s_max_size = msb->s_max_size;
  sb->s_magic = msb->s_magic;
  sb->s_dev = dev;
  sb->s_op = &minix_sops;

  /* reset maps */
  for (i = 0; i < msb->s_imap_blocks; i++)
    sb->s_imap[i] = NULL;
  for (i = 0; i < msb->s_zmap_blocks; i++)
    sb->s_zmap[i] = NULL;

  /* read imap blocks */
  block = 2;
  for (i = 0; i < msb->s_imap_blocks; i++, block++) {
    sb->s_imap[i] = bread(dev, block);
    if (!sb->s_imap[i]) {
      ret = -ENOMEM;
      goto err_map;
    }
  }

  /* read zmap blocks */
  for (i = 0; i < msb->s_zmap_blocks; i++, block++) {
    sb->s_zmap[i] = bread(dev, block);
    if (!sb->s_zmap[i]) {
      ret = -ENOMEM;
      goto err_map;
    }
  }

  /* read root inode */
  sb->s_imount = iget(sb, MINIX_ROOT_INODE);
  if (!sb->s_imount) {
    ret = -EINVAL;
    goto err_root_inode;
  }

  return 0;
err_root_inode:
  printf("[Minix-fs] Can't read root inode\n");
  goto release_map;
err_map:
  printf("[Minix-fs] Can't read imap and zmap\n");
release_map:
  for (i = 0; i < msb->s_imap_blocks; i++)
    brelse(sb->s_imap[i]);
  for (i = 0; i < msb->s_zmap_blocks; i++)
    brelse(sb->s_zmap[i]);
  goto err;
err_magic:
  printf("[Minix-fs] Bad magic number\n");
  goto err;
bad_sb:
  printf("[Minix-fs] Can't read super block\n");
err:
  return ret;
}

