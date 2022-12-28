#ifndef _TMP_FS_H_
#define _TMP_FS_H_

#include <fs/fs.h>

#define TMPFS_SUPER_MAGIC		0x1994
#define TMPFS_NAME_LEN			60
#define TMPFS_ROOT_INO			1

/*
 * Tmpfs directory entry.
 */
struct tmp_dir_entry_t {
	ino_t			d_inode;
	char			d_name[TMPFS_NAME_LEN];
};

/* super operations */
int init_tmp_fs();
void tmp_statfs(struct super_block_t *sb, struct statfs64_t *buf);

/* inode operations */
struct inode_t *tmp_new_inode(struct super_block_t *sb, mode_t mode, dev_t dev);
int tmp_read_inode(struct inode_t *inode);
int tmp_write_inode(struct inode_t *inode);
int tmp_put_inode(struct inode_t *inode);
void tmp_truncate(struct inode_t *inode);
int tmp_inode_grow_size(struct inode_t *inode, size_t size);

/* directory operations */
int tmp_getdents64(struct file_t *filp, void *dirp, size_t count);

/* name resolution operations */
int tmp_add_entry(struct inode_t *dir, const char *name, int name_len, struct inode_t *inode);
int tmp_lookup(struct inode_t *dir, const char *name, size_t name_len, struct inode_t **res_inode);
int tmp_create(struct inode_t *dir, const char *name, size_t name_len, mode_t mode, struct inode_t **res_inode);
int tmp_link(struct inode_t *old_inode, struct inode_t *dir, const char *name, size_t name_len);
int tmp_unlink(struct inode_t *dir, const char *name, size_t name_len);
int tmp_symlink(struct inode_t *dir, const char *name, size_t name_len, const char *target);
int tmp_mkdir(struct inode_t *dir, const char *name, size_t name_len, mode_t mode);
int tmp_rmdir(struct inode_t *dir, const char *name, size_t name_len);
int tmp_rename(struct inode_t *old_dir, const char *old_name, size_t old_name_len,
	       struct inode_t *new_dir, const char *new_name, size_t new_name_len);
int tmp_mknod(struct inode_t *dir, const char *name, size_t name_len, mode_t mode, dev_t dev);

/* file operations */
int tmp_file_read(struct file_t *filp, char *buf, int count);
int tmp_file_write(struct file_t *filp, const char *buf, int count);
int tmp_readpage(struct inode_t *inode, struct page_t *page);

/* symbolic link operations */
int tmp_follow_link(struct inode_t *dir, struct inode_t *inode, int flags, mode_t mode, struct inode_t **res_inode);
ssize_t tmp_readlink(struct inode_t *inode, char *buf, size_t bufsize);

#endif
