#include <fs/fs.h>
#include <proc/sched.h>
#include <mm/mm.h>
#include <stdio.h>
#include <stderr.h>
#include <string.h>

/* root fs super block */
struct minix_super_block_t *root_sb = NULL;

/*
 * Mount root device.
 */
int mount_root(struct ata_device_t *dev)
{
  struct minix_super_block_t *sb;
  struct buffer_head_t *bh;
  uint32_t block;
  int i, ret;

  /* read super block */
  bh = bread(dev, 1);
  if (!bh)
    goto bad_sb;

  /* set super block */
  sb = (struct minix_super_block_t *) bh->b_data;
  root_sb = sb;

  /* check magic number */
  if (sb->s_magic != MINIX_SUPER_MAGIC) {
    ret = EINVAL;
    goto err_magic;
  }

  /* set super block device */
  sb->s_dev = dev;

  /* reset maps */
  for (i = 0; i < sb->s_imap_blocks; i++)
    sb->s_imap[i] = NULL;
  for (i = 0; i < sb->s_zmap_blocks; i++)
    sb->s_zmap[i] = NULL;

  /* read imap blocks */
  block = 2;
  for (i = 0; i < sb->s_imap_blocks; i++, block++) {
    sb->s_imap[i] = bread(dev, block);
    if (!sb->s_imap[i]) {
      ret = ENOMEM;
      goto err_map;
    }
  }

  /* read zmap blocks */
  for (i = 0; i < sb->s_zmap_blocks; i++, block++) {
    sb->s_zmap[i] = bread(dev, block);
    if (!sb->s_zmap[i]) {
      ret = ENOMEM;
      goto err_map;
    }
  }

  /* read root inode */
  sb->s_imount = iget(sb, MINIX_ROOT_INODE);
  if (!sb->s_imount) {
    ret = EINVAL;
    goto err_root_inode;
  }

  return 0;
err_root_inode:
  printf("[Minix-fs] Can't read root inode\n");
  if (sb->s_imount)
    kfree(sb->s_imount);
  goto release_map;
err_map:
  printf("[Minix-fs] Can't read imap and zmap\n");
release_map:
  for (i = 0; i < sb->s_imap_blocks; i++)
    brelse(sb->s_imap[i]);
  for (i = 0; i < sb->s_zmap_blocks; i++)
    brelse(sb->s_zmap[i]);
  goto release_sb;
err_magic:
  printf("[Minix-fs] Bad magic number\n");
release_sb:
  kfree(sb);
  goto err;
bad_sb:
  printf("[Minix-fs] Can't read super block\n");
err:
  return ret;
}
