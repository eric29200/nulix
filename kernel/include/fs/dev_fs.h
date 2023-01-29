#ifndef _DEVFS_FS_H_
#define _DEVFS_FS_H_

#include <fs/fs.h>

#define DEVFS_SUPER_MAGIC		0x1995
#define DEVFS_ROOT_INO			1

/*
 * Devfs device.
 */
struct devfs_dev_t {
	dev_t				dev;
};

/*
 * Devfs directory.
 */
struct devfs_dir_t {
	struct list_head_t		entries;
};

/*
 * Devfs symbolic link.
 */
struct devfs_symlink_t {
	size_t				linkname_len;
	char *				linkname;
};

/*
 * Devfs entry.
 */
struct devfs_entry_t {
	ino_t				ino;
	char *				name;
	size_t				name_len;
	mode_t				mode;
	struct devfs_entry_t *		parent;
	struct list_head_t		list;
	union {
		struct devfs_dev_t	dev;
		struct devfs_dir_t	dir;
		struct devfs_symlink_t	symlink;
	} u;
};

/* super operations */
int init_dev_fs();

/* base operations */
struct devfs_entry_t *devfs_get_root_entry();
struct devfs_entry_t *devfs_find(struct devfs_entry_t *dir, const char *name, size_t name_len);
struct devfs_entry_t *devfs_register(struct devfs_entry_t *dir, const char *name, mode_t mode, dev_t dev);
void devfs_unregister(struct devfs_entry_t *de);
struct devfs_entry_t *devfs_mkdir(struct devfs_entry_t *dir, const char *name);
struct devfs_entry_t *devfs_symlink(struct devfs_entry_t *dir, const char *name, const char *linkname);

/* inode operations */
struct inode_t *devfs_iget(struct super_block_t *sb, struct devfs_entry_t *de);
int devfs_read_inode(struct inode_t *inode);
int devfs_write_inode(struct inode_t *inode);
int devfs_put_inode(struct inode_t *inode);

/* directory operations */
int devfs_getdents64(struct file_t *filp, void *dirp, size_t count);

/* name resolution operations */
int devfs_lookup(struct inode_t *dir, const char *name, size_t name_len, struct inode_t **res_inode);

/* symbolic link operations */
int devfs_follow_link(struct inode_t *dir, struct inode_t *inode, int flags, mode_t mode, struct inode_t **res_inode);
ssize_t devfs_readlink(struct inode_t *inode, char *buf, size_t bufsize);

#endif