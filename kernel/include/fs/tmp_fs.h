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
int tmpfs_lookup(struct inode *dir, struct dentry *dentry);
int tmpfs_create(struct inode *dir, struct dentry *dentry, mode_t mode);
int tmpfs_link(struct inode *inode, struct inode *dir, struct dentry *dentry);
int tmpfs_unlink(struct inode *dir, struct dentry *dentry);
int tmpfs_symlink(struct inode *dir, struct dentry *dentry, const char *target);
int tmpfs_mkdir(struct inode *dir, struct dentry *dentry, mode_t mode);
int tmpfs_rmdir(struct inode *dir, struct dentry *dentry);
int tmpfs_rename(struct inode *old_dir, struct dentry *old_dentry, struct inode *new_dir, struct dentry *new_dentry);
int tmpfs_mknod(struct inode *dir, struct dentry *dentry, mode_t mode, dev_t dev);

/* file operations */
int tmpfs_file_read(struct file *filp, char *buf, int count);
int tmpfs_file_write(struct file *filp, const char *buf, int count);
int tmpfs_readpage(struct inode *inode, struct page *page);

/* symbolic link operations */
struct dentry *tmpfs_follow_link(struct inode *inode, struct dentry *base);
ssize_t tmpfs_readlink(struct inode *inode, char *buf, size_t bufsize);

#endif
