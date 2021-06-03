#include <fs/proc_fs.h>
#include <stderr.h>
#include <stdio.h>

/*
 * Superblock operations.
 */
static struct super_operations_t proc_sops = {
  .read_inode     = proc_read_inode,
  .write_inode    = proc_write_inode,
  .put_inode      = proc_put_inode,
};

/*
 * Read super block.
 */
int proc_read_super(struct super_block_t *sb)
{
  /* set super block */
  sb->s_magic = PROC_SUPER_MAGIC;
  sb->s_op = &proc_sops;

  /* get root inode */
  sb->s_mounted = iget(sb, PROC_ROOT_INO);
  if (!sb->s_mounted) {
    printf("[Proc-fs] Can't get root inode\n");
    return -EACCES;
  }

  return 0;
}
