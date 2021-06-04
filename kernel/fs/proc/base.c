#include <fs/proc_fs.h>

static int proc_base_getdents64(struct file_t *filp, void *dirp, size_t count)
{
  return 0;
}

static int proc_base_lookup(struct inode_t *dir, const char *name, size_t name_len, struct inode_t **res_inode)
{
  return 0;
}

/*
 * Process file operations.
 */
struct file_operations_t proc_base_fops = {
  .getdents64     = proc_base_getdents64,
};

/*
 * Process inode operations.
 */
struct inode_operations_t proc_base_iops = {
  .fops           = &proc_base_fops,
  .lookup         = proc_base_lookup,
};

