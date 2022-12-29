#ifndef _TMPFS_FS_H_
#define _TMPFS_FS_H_

#include <fs/fs.h>

#define TMPFS_SUPER_MAGIC		0x1994
#define TMPFS_NAME_LEN			60
#define TMPFS_ROOT_INO			1

/*
 * Tmpfs directory entry.
 */
struct tmpfs_dir_entry_t {
	ino_t			d_inode;
	char			d_name[TMPFS_NAME_LEN];
};

/* super operations */
int init_tmp_fs();

/* inode operations */
struct inode_t *tmpfs_new_inode(struct super_block_t *sb, mode_t mode, dev_t dev);
int tmpfs_read_inode(struct inode_t *inode);
int tmpfs_write_inode(struct inode_t *inode);
int tmpfs_put_inode(struct inode_t *inode);
void tmpfs_truncate(struct inode_t *inode);
int tmpfs_inode_grow_size(struct inode_t *inode, size_t size);

/* directory operations */
int tmpfs_getdents64(struct file_t *filp, void *dirp, size_t count);

/* name resolution operations */
int tmpfs_add_entry(struct inode_t *dir, const char *name, int name_len, struct inode_t *inode);
int tmpfs_lookup(struct inode_t *dir, const char *name, size_t name_len, struct inode_t **res_inode);
int tmpfs_create(struct inode_t *dir, const char *name, size_t name_len, mode_t mode, struct inode_t **res_inode);
int tmpfs_link(struct inode_t *old_inode, struct inode_t *dir, const char *name, size_t name_len);
int tmpfs_unlink(struct inode_t *dir, const char *name, size_t name_len);
int tmpfs_symlink(struct inode_t *dir, const char *name, size_t name_len, const char *target);
int tmpfs_mkdir(struct inode_t *dir, const char *name, size_t name_len, mode_t mode);
int tmpfs_rmdir(struct inode_t *dir, const char *name, size_t name_len);
int tmpfs_rename(struct inode_t *old_dir, const char *old_name, size_t old_name_len,
	         struct inode_t *new_dir, const char *new_name, size_t new_name_len);
int tmpfs_mknod(struct inode_t *dir, const char *name, size_t name_len, mode_t mode, dev_t dev);

/* file operations */
int tmpfs_file_read(struct file_t *filp, char *buf, int count);
int tmpfs_file_write(struct file_t *filp, const char *buf, int count);
int tmpfs_readpage(struct inode_t *inode, struct page_t *page);

/* symbolic link operations */
int tmpfs_follow_link(struct inode_t *dir, struct inode_t *inode, int flags, mode_t mode, struct inode_t **res_inode);
ssize_t tmpfs_readlink(struct inode_t *inode, char *buf, size_t bufsize);

#endif
