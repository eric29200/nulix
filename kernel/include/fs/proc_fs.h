#ifndef _PROC_FS_H_
#define _PROC_FS_H_

#include <fs/fs.h>

#define PROC_SUPER_MAGIC      0x9FA0

#define PROC_ROOT_INO         1
#define PROC_UPTIME_INO       2

/*
 * Procfs dir entry.
 */
struct proc_dir_entry_t {
  ino_t   ino;
  size_t  name_len;
  char *  name;
};

/* read super block */
int proc_read_super(struct super_block_t *sb);

/* super block operations */
int proc_read_inode(struct inode_t *inode);
int proc_write_inode(struct inode_t *inode);
int proc_put_inode(struct inode_t *inode);

/* inode operations */
extern struct inode_operations_t proc_root_iops;
extern struct inode_operations_t proc_uptime_iops;

#endif
