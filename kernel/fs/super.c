#include <fs/minix_fs.h>
#include <fs/proc_fs.h>
#include <proc/sched.h>
#include <mm/mm.h>
#include <stdio.h>
#include <stderr.h>
#include <string.h>
#include <fcntl.h>

/*
 * Mount a file system.
 */
int do_mount(uint16_t magic, struct ata_device_t *dev, const char *mount_point)
{
  struct super_block_t *sb;
  struct inode_t *dir;
  int err;

  /* get mount point */
  dir = namei(AT_FDCWD, mount_point, 1);
  if (!dir)
    return -EINVAL;

  /* mount point busy */
  if (dir->i_ref != 1 || dir->i_mount) {
    iput(dir);
    return -EBUSY;
  }

  /* mount point must be a directory */
  if (!S_ISDIR(dir->i_mode)) {
    iput(dir);
    return -EPERM;
  }

  /* allocate a super block */
  sb = (struct super_block_t *) kmalloc(sizeof(struct super_block_t));
  if (!sb) {
    iput(dir);
    return -ENOMEM;
  }

  /* read super block */
  switch (magic) {
    case MINIX_SUPER_MAGIC:
      err = minix_read_super(sb, dev);
      break;
    case PROC_SUPER_MAGIC:
      err = proc_read_super(sb);
      break;
    default:
      err = -EINVAL;
      break;
  }

  /* no way to read super block */
  if (err) {
    kfree(sb);
    iput(dir);
    return err;
  }

  /* set mount point */
  sb->s_covered = dir;
  dir->i_mount = sb->s_mounted;

  return 0;
}

/*
 * Mount root device.
 */
int mount_root(struct ata_device_t *dev)
{
  struct super_block_t *root_sb;
  struct inode_t *inode;
  int err;

  /* init buffers */
  binit();

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
