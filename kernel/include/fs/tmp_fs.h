#ifndef _TMPFS_FS_H_
#define _TMPFS_FS_H_

#include <fs/fs.h>

#define TMPFS_SUPER_MAGIC		0x1994
#define TMPFS_NAME_LEN			60
#define TMPFS_ROOT_INO			1

/*
 * Tmpfs directory entry.
 */
struct tmpfs_dir_entry {
	ino_t			d_inode;
	char			d_name[TMPFS_NAME_LEN];
};

/* super operations */
int init_tmp_fs();

/* inode operations */
struct inode *tmpfs_new_inode(struct super_block *sb, mode_t mode, dev_t dev);
int tmpfs_read_inode(struct inode *inode);
int tmpfs_write_inode(struct inode *inode);
int tmpfs_put_inode(struct inode *inode);
void tmpfs_truncate(struct inode *inode);
int tmpfs_inode_grow_size(struct inode *inode, size_t size);

/* directory operations */
int tmpfs_getdents64(struct file *filp, void *dirp, size_t count);

/* name resolution operations */
int tmpfs_add_entry(struct inode *dir, const char *name, int name_len, struct inode *inode);
int tmpfs_lookup(struct inode *dir, const char *name, size_t name_len, struct inode **res_inode);
int tmpfs_create(struct inode *dir, const char *name, size_t name_len, mode_t mode, struct inode **res_inode);
int tmpfs_link(struct inode *inode, struct inode *dir, struct dentry *dentry);
int tmpfs_unlink(struct inode *dir, struct dentry *dentry);
int tmpfs_symlink(struct inode *dir, const char *name, size_t name_len, const char *target);
int tmpfs_mkdir(struct inode *dir, struct dentry *dentry, mode_t mode);
int tmpfs_rmdir(struct inode *dir, struct dentry *dentry);
int tmpfs_rename(struct inode *old_dir, const char *old_name, size_t old_name_len,
	         struct inode *new_dir, const char *new_name, size_t new_name_len);
int tmpfs_mknod(struct inode *dir, struct dentry *dentry, mode_t mode, dev_t dev);

/* file operations */
int tmpfs_file_read(struct file *filp, char *buf, int count);
int tmpfs_file_write(struct file *filp, const char *buf, int count);
int tmpfs_readpage(struct inode *inode, struct page *page);

/* symbolic link operations */
int tmpfs_follow_link(struct inode *dir, struct inode *inode, int flags, mode_t mode, struct inode **res_inode);
ssize_t tmpfs_readlink(struct inode *inode, char *buf, size_t bufsize);

#endif
