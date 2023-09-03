#ifndef _DEVFS_FS_H_
#define _DEVFS_FS_H_

#include <fs/fs.h>

#define DEVFS_SUPER_MAGIC		0x1995
#define DEVFS_ROOT_INO			1

/*
 * Devfs device.
 */
struct devfs_dev {
	dev_t				dev;
};

/*
 * Devfs directory.
 */
struct devfs_dir {
	struct list_head		entries;
};

/*
 * Devfs symbolic link.
 */
struct devfs_symlink {
	size_t				linkname_len;
	char *				linkname;
};

/*
 * Devfs entry.
 */
struct devfs_entry {
	ino_t				ino;
	char *				name;
	size_t				name_len;
	mode_t				mode;
	struct devfs_entry *		parent;
	struct list_head		list;
	union {
		struct devfs_dev	dev;
		struct devfs_dir	dir;
		struct devfs_symlink	symlink;
	} u;
};

/* super operations */
int init_dev_fs();

/* base operations */
struct devfs_entry *devfs_get_root_entry();
struct devfs_entry *devfs_find(struct devfs_entry *dir, const char *name, size_t name_len);
struct devfs_entry *devfs_register(struct devfs_entry *dir, const char *name, mode_t mode, dev_t dev);
void devfs_unregister(struct devfs_entry *de);
struct devfs_entry *devfs_mkdir(struct devfs_entry *dir, const char *name);
struct devfs_entry *devfs_symlink(struct devfs_entry *dir, const char *name, const char *linkname);

/* inode operations */
struct inode *devfs_iget(struct super_block *sb, struct devfs_entry *de);
int devfs_read_inode(struct inode *inode);
int devfs_write_inode(struct inode *inode);
int devfs_put_inode(struct inode *inode);

/* directory operations */
int devfs_getdents64(struct file *filp, void *dirp, size_t count);

/* name resolution operations */
int devfs_lookup(struct inode *dir, const char *name, size_t name_len, struct inode **res_inode);

/* symbolic link operations */
int devfs_follow_link(struct inode *dir, struct inode *inode, int flags, mode_t mode, struct inode **res_inode);
ssize_t devfs_readlink(struct inode *inode, char *buf, size_t bufsize);

#endif
