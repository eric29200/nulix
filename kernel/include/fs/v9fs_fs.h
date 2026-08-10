#ifndef _V9FS_FS_H_
#define _V9FS_FS_H_

#include <stddef.h>

#define V9FS_PORT		564
#define V9FS_DEFUSER		"nobody"
#define V9FS_DEFANAME		""

#define V9FS_SUPER_MAGIC	0x01021997

/*
 * 9p session.
 */
struct v9fs_session_info {
	char *			uname;
	char *			aname;
	struct p9_client *	client;
	uint32_t		maxdata;
};

/* 9p super operations */
int init_v9fs_fs();

/* inode operations */
int v9fs_read_inode(struct inode *inode);
int v9fs_put_inode(struct inode *inode);
struct inode *v9fs_get_inode(struct super_block *sb, int mode);

/* name resolution operations */
struct dentry *v9fs_lookup(struct inode *dir, struct dentry *dentry);

/* read directory operations */
int v9fs_readdir(struct file *filp, void *dirent, filldir_t filldir);

#endif