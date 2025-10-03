#ifndef _DEVPTS_FS_H_
#define _DEVPTS_FS_H_

#include <fs/fs.h>

#define DEVPTS_SUPER_MAGIC		0x1995
#define DEVPTS_ROOT_INO			1

/*
 * Devpts device.
 */
struct devpts_dev {
	dev_t				dev;
};

/*
 * Devpts directory.
 */
struct devpts_dir {
	struct list_head		entries;
};

/*
 * Devpts entry.
 */
struct devpts_entry {
	ino_t				ino;
	char *				name;
	size_t				name_len;
	mode_t				mode;
	struct devpts_entry *		parent;
	struct list_head		list;
	union {
		struct devpts_dev	dev;
		struct devpts_dir	dir;
	} u;
};

/* super operations */
int init_devpts_fs();

/* base operations */
struct devpts_entry *devpts_get_root_entry();
struct devpts_entry *devpts_find(struct devpts_entry *dir, const char *name, size_t name_len);
struct devpts_entry *devpts_register(const char *name, mode_t mode, dev_t dev);
void devpts_unregister(struct devpts_entry *de);

/* inode operations */
struct inode *devpts_iget(struct super_block *sb, struct devpts_entry *de);
int devpts_read_inode(struct inode *inode);
int devpts_write_inode(struct inode *inode);
int devpts_put_inode(struct inode *inode);

/* directory operations */
int devpts_readdir(struct file *filp, void *dirp, size_t count);

/* name resolution operations */
struct dentry *devpts_lookup(struct inode *dir, struct dentry *dentry);

#endif
