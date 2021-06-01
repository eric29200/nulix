#include <fs/fs.h>
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

  /* set current task current working dir to root */
  current_task->cwd = root_sb->s_imount;
  strcpy(current_task->cwd_path, "/");
  current_task->root = root_sb->s_imount;

  return 0;
}
