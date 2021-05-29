#include <fs/fs.h>
#include <proc/sched.h>
#include <mm/mm.h>
#include <stdio.h>
#include <stderr.h>
#include <string.h>

/* root fs super block */
static struct super_block_t *root_sb = NULL;

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

  /* read minix super block */
  sb = (struct minix_super_block_t *) bh->b_data;

  /* check magic number */
  if (sb->s_magic != MINIX_SUPER_MAGIC) {
    ret = EINVAL;
    goto err_magic;
  }

  /* allocate root super block */
  root_sb = (struct super_block_t *) kmalloc(sizeof(struct super_block_t));
  if (!root_sb) {
    ret = -ENOMEM;
    goto err_mem_sb;
  }

  /* set root super block */
  root_sb->s_ninodes = sb->s_ninodes;
  root_sb->s_nzones = sb->s_nzones;
  root_sb->s_imap_blocks = sb->s_imap_blocks;
  root_sb->s_zmap_blocks = sb->s_zmap_blocks;
  root_sb->s_firstdatazone = sb->s_firstdatazone;
  root_sb->s_log_zone_size = sb->s_log_zone_size;
  root_sb->s_max_size = sb->s_max_size;
  root_sb->s_magic = sb->s_magic;
  root_sb->s_dev = dev;

  /* reset maps */
  for (i = 0; i < sb->s_imap_blocks; i++)
    root_sb->s_imap[i] = NULL;
  for (i = 0; i < sb->s_zmap_blocks; i++)
    root_sb->s_zmap[i] = NULL;

  /* read imap blocks */
  block = 2;
  for (i = 0; i < sb->s_imap_blocks; i++, block++) {
    root_sb->s_imap[i] = bread(dev, block);
    if (!root_sb->s_imap[i]) {
      ret = ENOMEM;
      goto err_map;
    }
  }

  /* read zmap blocks */
  for (i = 0; i < sb->s_zmap_blocks; i++, block++) {
    root_sb->s_zmap[i] = bread(dev, block);
    if (!root_sb->s_zmap[i]) {
      ret = ENOMEM;
      goto err_map;
    }
  }

  /* read root inode */
  root_sb->s_imount = iget(root_sb, MINIX_ROOT_INODE);
  if (!root_sb->s_imount) {
    ret = EINVAL;
    goto err_root_inode;
  }

  /* set current task current working dir to root */
  current_task->cwd = root_sb->s_imount;
  current_task->root = root_sb->s_imount;

  return 0;
err_root_inode:
  printf("[Minix-fs] Can't read root inode\n");
  if (root_sb->s_imount)
    kfree(root_sb->s_imount);
  goto release_map;
err_map:
  printf("[Minix-fs] Can't read imap and zmap\n");
release_map:
  for (i = 0; i < sb->s_imap_blocks; i++)
    brelse(root_sb->s_imap[i]);
  for (i = 0; i < sb->s_zmap_blocks; i++)
    brelse(root_sb->s_zmap[i]);
  goto release_sb;
err_mem_sb:
  printf("[Minix-fs] Can't allocate superblock\n");
release_sb:
  kfree(root_sb);
  goto err;
err_magic:
  printf("[Minix-fs] Bad magic number\n");
bad_sb:
  printf("[Minix-fs] Can't read super block\n");
err:
  return ret;
}
