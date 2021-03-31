
#ifndef _KFS_H_
#define _KFS_H_

#include <lib/stddef.h>

#define KFS_MAGIC                   0xA0A0A0A0
#define KFS_BLOCK_SIZE              4096
#define KFS_FILENAME_LEN            64
#define KFS_ROOT_INODE_NUMBER       1
#define KFS_SUPERBLOCK_BLOCK_NUMBER 0
#define KFS_INODESTORE_BLOCK_NUMBER 1
#define KFS_DATA_BLOCK_NUMBER       2

#define KFS_MAX_INODES              (KFS_BLOCK_SIZE / sizeof(struct kfs_inode_t))

/*
 * Kfs super block.
 */
struct kfs_superblock_t {
  uint32_t s_magic;
  uint32_t s_block_size;
};

/*
 * Kfs inode.
 */
struct kfs_inode_t {
  uint32_t i_mode;
  uint32_t i_inode_no;
  uint32_t i_block;
  union {
    uint32_t i_size;
    uint32_t i_nb_children;
  };
};

/*
 * Kfs dir record.
 */
struct kfs_dir_record_t {
  char d_name[KFS_FILENAME_LEN];
  uint32_t d_inode_no;
};

int kfs_mount(uint32_t location);

#endif
