#include <fs/minix_fs.h>
#include <fs/buffer.h>
#include <mm/mm.h>
#include <stdio.h>
#include <stderr.h>

static struct minix_super_block_t *root_sb = NULL;

/*
 * Read an inode.
 */
static struct inode_t *read_inode(struct minix_super_block_t *sb, ino_t ino)
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
  init_sem(&inode->i_sem);

  /* free minix inode */
  kfree(minix_inode);

  return inode;
}

/*
 * Mount root device.
 */
int mount_root(struct ata_device_t *dev)
{
  struct minix_super_block_t *sb;
  uint32_t block;
  int i, ret;

  /* read super block */
  sb = (struct minix_super_block_t *) bread(dev, 1);
  root_sb = sb;

  /* check magic number */
  if (sb->s_magic != MINIX_SUPER_MAGIC) {
    ret = EINVAL;
    goto err_magic;
  }

  /* set super block device */
  sb->s_dev = dev;
  sb->s_imap = NULL;
  sb->s_zmap = NULL;

  /* allocate imap blocks */
  sb->s_imap = (char **) kmalloc(sizeof(char *) * sb->s_imap_blocks);
  if (!sb->s_imap) {
    ret = ENOMEM;
    goto err_map;
  }

  /* allocate zmap blocks */
  sb->s_zmap = (char **) kmalloc(sizeof(char *) * sb->s_zmap_blocks);
  if (!sb->s_imap) {
    ret = ENOMEM;
    goto err_map;
  }

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
  sb->s_imount = read_inode(sb, 1);
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
  if (sb->s_imap)
    for (i = 0; i < sb->s_imap_blocks; i++)
      if (sb->s_imap[i])
        kfree(sb->s_imap[i]);
  if (sb->s_zmap)
    for (i = 0; i < sb->s_zmap_blocks; i++)
      if (sb->s_zmap[i])
        kfree(sb->s_zmap[i]);
  goto release_sb;
err_magic:
  printf("[Minix-fs] Bad magic number\n");
release_sb:
  kfree(sb);
  return ret;
}
