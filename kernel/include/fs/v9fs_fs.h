#ifndef _V9FS_FS_H_
#define _V9FS_FS_H_

#include <stddef.h>
#include <net/9p/9p.h>

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

/*
 * 9p private data stored in dentry d_private.
 */
struct v9fs_dentry {
	struct list_head	fid_list;
};

/* 9p super operations */
int init_v9fs_fs();

/* inode operations */
struct inode *v9fs_get_inode(struct super_block *sb, int mode);
void v9fs_stat2inode(struct p9_stat *stat, struct inode *inode);
struct inode *v9fs_get_inode_from_fid(struct p9_fid *fid, struct super_block *sb);

/* fid operations */
int v9fs_fid_add(struct dentry *dentry, struct p9_fid *fid);
struct p9_fid *v9fs_fid_lookup(struct dentry *dentry);
struct p9_fid *v9fs_fid_clone(struct dentry *dentry);

/* name resolution operations */
struct dentry *v9fs_lookup(struct inode *dir, struct dentry *dentry);

/* read directory operations */
int v9fs_readdir(struct file *filp, void *dirent, filldir_t filldir);

/* file operations */
int v9fs_open(struct inode *inode, struct file *file);
int v9fs_release(struct inode *inode, struct file *filp);
int v9fs_file_read(struct file *filp, char *buf, size_t count, off_t *offset);

/*
 * Convert qid into inode number.
 */
static inline ino_t v9fs_qid2ino(struct p9_qid *qid)
{
	uint64_t path = qid->path + 2;
	return (ino_t) (path ^ (path >> 32));
}

#endif