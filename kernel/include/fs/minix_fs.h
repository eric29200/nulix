#ifndef _MINIX_FS_H_
#define _MINIX_FS_H_

#include <fs/fs.h>

#define MINIX_SUPER_MAGIC             0x2478
#define MINIX_ROOT_INODE              1
#define MINIX_FILENAME_LEN            30
#define MINIX_INODES_PER_BLOCK        ((BLOCK_SIZE) / (sizeof(struct minix_inode_t)))
#define MINIX_DIR_ENTRIES_PER_BLOCK   ((BLOCK_SIZE) / (sizeof(struct minix_dir_entry_t)))

/*
 * Minix super block.
 */
struct minix_super_block_t {
  uint16_t                s_ninodes;
  uint16_t                s_nzones;
  uint16_t                s_imap_blocks;
  uint16_t                s_zmap_blocks;
  uint16_t                s_firstdatazone;
  uint16_t                s_log_zone_size;
  uint32_t                s_max_size;
  uint16_t                s_magic;
  uint16_t                s_state;
  uint32_t                s_zones;
};

/*
 * Minix inode.
 */
struct minix_inode_t {
  uint16_t                i_mode;
  uint16_t                i_nlinks;
  uint16_t                i_uid;
  uint16_t                i_gid;
  uint32_t                i_size;
  uint32_t                i_atime;
  uint32_t                i_mtime;
  uint32_t                i_ctime;
  uint32_t                i_zone[10];
};

/*
 * Minix dir entry.
 */
struct minix_dir_entry_t {
  uint16_t  inode;
  char      name[MINIX_FILENAME_LEN];
};

/* minix operations */
extern struct super_operations_t minix_sops;
extern struct inode_operations_t minix_file_iops;
extern struct inode_operations_t minix_dir_iops;
extern struct inode_operations_t minix_char_iops;
extern struct file_operations_t minix_file_fops;
extern struct file_operations_t minix_dir_fops;

/* minix super operations */
int minix_read_super(struct super_block_t *sb, dev_t dev);
int minix_read_inode(struct inode_t *inode);
int minix_write_inode(struct inode_t *inode);
int minix_put_inode(struct inode_t *inode);
struct inode_t *minix_new_inode(struct super_block_t *sb);
int minix_free_inode(struct inode_t *inode);
uint32_t minix_new_block(struct super_block_t *sb);
int minix_free_block(struct super_block_t *sb, uint32_t block);
void minix_truncate(struct inode_t *inode);
struct buffer_head_t *minix_bread(struct inode_t *inode, int block, int create);

/* minix inode operations */
int minix_lookup(struct inode_t *dir, const char *name, size_t name_len, struct inode_t **res_inode);
int minix_create(struct inode_t *dir, const char *name, size_t name_len, mode_t mode, struct inode_t **res_inode);
int minix_follow_link(struct inode_t *inode, struct inode_t **res_inode);
ssize_t minix_readlink(struct inode_t *inode, char *buf, size_t bufsize);
int minix_link(struct inode_t *old_inode, struct inode_t *dir, const char *name, size_t name_len);
int minix_unlink(struct inode_t *dir, const char *name, size_t name_len);
int minix_symlink(struct inode_t *dir, const char *name, size_t name_len, const char *target);
int minix_mkdir(struct inode_t *dir, const char *name, size_t name_len, mode_t mode);
int minix_rmdir(struct inode_t *dir, const char *name, size_t name_len);
int minix_rename(struct inode_t *old_dir, const char *old_name, size_t old_name_len,
                 struct inode_t *new_dir, const char *new_name, size_t new_name_len);
int minix_mknod(struct inode_t *dir, const char *name, size_t name_len, mode_t mode, dev_t dev);

/* minix directory operations */
int minix_getdents64(struct file_t *filp, void *dirp, size_t count);

/* minix file operations */
int minix_file_read(struct file_t *filp, char *buf, int count);
int minix_file_write(struct file_t *filp, const char *buf, int count);

#endif
