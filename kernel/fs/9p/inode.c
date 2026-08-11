#include <fs/fs.h>
#include <fs/v9fs_fs.h>
#include <proc/sched.h>
#include <net/9p/9p.h>
#include <stdio.h>
#include <stderr.h>
#include <fcntl.h>

/*
 * Read an inode.
 */
int v9fs_read_inode(struct inode *inode)
{
	UNUSED(inode);
	printf("TODO: v9fs_read_inode\n");
	return -EINVAL;
}

/*
 * Release an inode.
 */
int v9fs_put_inode(struct inode *inode)
{
	UNUSED(inode);
	printf("TODO: v9fs_put_inode\n");
	return -EINVAL;
}

/*
 * Directory operations.
 */
static struct file_operations v9fs_dir_fops = {
	.readdir	= v9fs_readdir,
};

/*
 * Directory inode operations.
 */
static struct inode_operations v9fs_dir_iops = {
	.fops		= &v9fs_dir_fops,
	.lookup		= v9fs_lookup,
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