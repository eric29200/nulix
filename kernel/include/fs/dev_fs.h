#ifndef _DEVFS_FS_H_
#define _DEVFS_FS_H_

#include <fs/fs.h>

#define DEVFS_SUPER_MAGIC		0x1995
#define DEVFS_NAME_LEN			60
#define DEVFS_ROOT_INO			1

/*
 * Devfs directory entry.
 */
struct devfs_dir_entry_t {
	ino_t			d_inode;
	char			d_name[DEVFS_NAME_LEN];
};

/* super operations */
int init_dev_fs();

/* inode operations */
struct inode_t *devfs_new_inode(struct super_block_t *sb, mode_t mode, dev_t dev);
int devfs_read_inode(struct inode_t *inode);
int devfs_write_inode(struct inode_t *inode);
int devfs_put_inode(struct inode_t *inode);
int devfs_inode_grow_size(struct inode_t *inode, size_t size);

/* directory operations */
int devfs_getdents64(struct file_t *filp, void *dirp, size_t count);

/* name resolution operations */
int devfs_add_entry(struct inode_t *dir, const char *name, int name_len, struct inode_t *inode);
int devfs_lookup(struct inode_t *dir, const char *name, size_t name_len, struct inode_t **res_inode);
int devfs_unlink(struct inode_t *dir, const char *name, size_t name_len);
int devfs_mkdir(struct inode_t *dir, const char *name, size_t name_len, mode_t mode);
int devfs_mknod(struct inode_t *dir, const char *name, size_t name_len, mode_t mode, dev_t dev);
int devfs_symlink(struct inode_t *dir, const char *name, size_t name_len, const char *target);

/* symbolic link operations */
int devfs_follow_link(struct inode_t *dir, struct inode_t *inode, int flags, mode_t mode, struct inode_t **res_inode);
ssize_t devfs_readlink(struct inode_t *inode, char *buf, size_t bufsize);

/* file operations */
int devfs_file_read(struct file_t *filp, char *buf, int count);

#endif