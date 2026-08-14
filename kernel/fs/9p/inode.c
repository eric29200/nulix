#include <fs/fs.h>
#include <fs/v9fs_fs.h>
#include <proc/sched.h>
#include <net/9p/9p.h>
#include <stdio.h>
#include <stderr.h>
#include <fcntl.h>

/*
 * Directory operations.
 */
static struct file_operations v9fs_dir_fops = {
	.open		= v9fs_open,
	.release	= v9fs_release,
	.readdir	= v9fs_readdir,
};

/*
 * Directory inode operations.
 */
static struct inode_operations v9fs_dir_iops = {
	.fops		= &v9fs_dir_fops,
	.lookup		= v9fs_lookup,
	.create		= v9fs_create,
	.mknod		= v9fs_mknod,
	.mkdir		= v9fs_mkdir,
	.link		= v9fs_link,
	.symlink	= v9fs_symlink,
	.unlink		= v9fs_unlink,
	.rmdir		= v9fs_rmdir,
};

/*
 * File operations.
 */
static struct file_operations v9fs_file_fops = {
	.open		= v9fs_open,
	.release	= v9fs_release,
	.read		= v9fs_file_read,
};

/*
 * File inode operations.
 */
static struct inode_operations v9fs_file_iops = {
	.fops		= &v9fs_file_fops,
};

/*
 * Symbolic link inode operations.
 */
static struct inode_operations v9fs_symlink_iops = {
	.follow_link	= v9fs_follow_link,
	.readlink	= v9fs_readlink,
};


/*
 * Init a 9p inode.
 */
int v9fs_init_inode(struct inode *inode, int mode)
{
	/* set inode */
	inode->i_mode = mode;
	inode->i_uid = current_task->fsuid;
	inode->i_gid = current_task->fsgid;
	inode->i_blocks = 0;
	inode->i_rdev = 0;
	inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;
	inode->i_nlinks = 1;

	/* set operations */
	switch (mode & S_IFMT) {
		case S_IFDIR:
			inode->i_nlinks++;
			inode->i_op = &v9fs_dir_iops;
			break;
		case S_IFREG:
			inode->i_op = &v9fs_file_iops;
			break;
		case S_IFLNK:
			inode->i_op = &v9fs_symlink_iops;
			break;
		default:
			p9_error("v9fs_init_inode: can't inode for mode 0x%x\n", mode & S_IFMT);
			return -EINVAL;
	}

	return 0;
}

/*
 * Get a 9p inode.
 */
struct inode *v9fs_get_inode(struct super_block *sb, int mode)
{
	struct inode *inode;
	int ret;

	/* get an empty inode */
	inode = get_empty_inode(sb);
	if (!inode)
		return ERR_PTR(-ENOMEM);

	/* init inode */
	ret = v9fs_init_inode(inode, mode);
	if (ret) {
		iput(inode);
		return ERR_PTR(ret);
	}

	return inode;
}

/*
 * Populate an inode structure with stat info
 */
void v9fs_stat2inode(struct p9_stat *stat, struct inode *inode)
{
	if ((stat->st_result_mask & P9_STATS_BASIC) == P9_STATS_BASIC) {
		inode->i_atime = sec_nsec_to_jiffies(stat->st_atime_sec, stat->st_atime_nsec);
		inode->i_mtime = sec_nsec_to_jiffies(stat->st_mtime_sec, stat->st_mtime_nsec);
		inode->i_ctime = sec_nsec_to_jiffies(stat->st_ctime_sec, stat->st_ctime_nsec);
		inode->i_uid = stat->st_uid;
		inode->i_gid = stat->st_gid;
		inode->i_nlinks = stat->st_nlink;
		inode->i_mode = stat->st_mode;
		inode->i_rdev = stat->st_rdev;

		if (S_ISCHR(inode->i_mode))
			inode->i_op = &chrdev_iops;
		else if (S_ISBLK(inode->i_mode))
			inode->i_op = &blkdev_iops;

		inode->i_size = stat->st_size;
		inode->i_blocks = stat->st_blocks;
	}

	if (stat->st_result_mask & P9_STATS_ATIME)
		inode->i_atime = sec_nsec_to_jiffies(stat->st_atime_sec, stat->st_atime_nsec);

	if (stat->st_result_mask & P9_STATS_MTIME)
		inode->i_mtime = sec_nsec_to_jiffies(stat->st_mtime_sec, stat->st_mtime_nsec);

	if (stat->st_result_mask & P9_STATS_CTIME)
		inode->i_ctime = sec_nsec_to_jiffies(stat->st_ctime_sec, stat->st_ctime_nsec);

	if (stat->st_result_mask & P9_STATS_UID)
		inode->i_uid = stat->st_uid;

	if (stat->st_result_mask & P9_STATS_GID)
		inode->i_gid = stat->st_gid;

	if (stat->st_result_mask & P9_STATS_NLINK)
		inode->i_nlinks = stat->st_nlink;

	if (stat->st_result_mask & P9_STATS_MODE) {
		inode->i_mode = stat->st_mode;
		if (S_ISCHR(inode->i_mode))
			inode->i_op = &chrdev_iops;
		else if (S_ISBLK(inode->i_mode))
			inode->i_op = &blkdev_iops;

	}

	if (stat->st_result_mask & P9_STATS_RDEV)
		inode->i_rdev = stat->st_rdev;

	if (stat->st_result_mask & P9_STATS_SIZE)
		inode->i_size = stat->st_size;

	if (stat->st_result_mask & P9_STATS_BLOCKS)
		inode->i_blocks = stat->st_blocks;
}

/*
 * Get an inode from a qid.
 */
static struct inode *v9fs_qid_iget(struct super_block *sb, struct p9_qid *qid, struct p9_stat *st)
{
	struct inode *inode;
	int ret;

	/* get an empty inode */
	inode = get_empty_inode(sb);
	if (!inode)
		return ERR_PTR(-ENOMEM);

	/* init inode */
	inode->i_ino = v9fs_qid2ino(qid);
	ret = v9fs_init_inode(inode, st->st_mode);
	if (ret)
		goto err;

	/* fill in inode */
	v9fs_stat2inode(st, inode);

	return inode;
err:
	iput(inode);
	return ERR_PTR(ret);
}

/*
 * Get an inode from a fid.
 */
struct inode *v9fs_get_inode_from_fid(struct p9_fid *fid, struct super_block *sb)
{
	struct inode *inode = NULL;
	struct p9_stat *st;

	/* get attributes */
	st = p9_client_getattr(fid, P9_STATS_BASIC);
	if (IS_ERR(st))
		return ERR_CAST(st);

	/* get inode */
	inode = v9fs_qid_iget(sb, &st->qid, st);
	kfree(st);

	return inode;
}

/*
 * Get dentry of an inode.
 */
struct dentry *v9fs_dentry_from_dir_inode(struct inode *inode)
{
	struct dentry *dentry;

	/* directory should have only one entry */
	if (S_ISDIR(inode->i_mode) && !list_is_singular(&inode->i_dentry))
		p9_fatal("v9fs_dentry_from_dir_inode: directory with multiple dentries\n");

	/* get first dentry of inode */
	dentry = list_first_entry(&inode->i_dentry, struct dentry, d_alias);

	return dentry;
}