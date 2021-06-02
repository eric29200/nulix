#include <fs/minix_fs.h>
#include <proc/sched.h>
#include <mm/mm.h>
#include <stdio.h>
#include <stderr.h>
#include <string.h>

/*
 * Mount root device.
 */
int mount_root(struct ata_device_t *dev)
{
  struct super_block_t *root_sb;
  struct inode_t *inode;
  int err;

  /* allocate root super block */
  root_sb = (struct super_block_t *) kmalloc(sizeof(struct super_block_t));
  if (!root_sb)
    return -ENOMEM;

  /* read super block */
  err = minix_read_super(root_sb, dev);
  if (err != 0) {
    kfree(root_sb);
    return err;
  }

  /* set mount point */
  inode = root_sb->s_mounted;
  inode->i_ref = 3;
  root_sb->s_covered = inode;

  /* set current task current working dir to root */
  current_task->cwd = inode;
  current_task->root = inode;
  strcpy(current_task->cwd_path, "/");

  return 0;
}
