#include <fs/dev_fs.h>
#include <proc/sched.h>
#include <fcntl.h>
#include <stderr.h>

/*
 * Directory operations.
 */
static struct file_operations_t devfs_dir_fops = {
	.getdents64		= devfs_getdents64,
};

/*
 * Directory inode operations.
 */
static struct inode_operations_t devfs_dir_iops = {
	.fops			= &devfs_dir_fops,
	.lookup			= devfs_lookup,
};

/*
 * Symbolic link inode operations.
 */
static struct inode_operations_t devfs_symlink_iops = {
	.follow_link		= devfs_follow_link,
	.readlink		= devfs_readlink,
};

/*
 * Read an inode.
 */
int devfs_read_inode(struct inode_t *inode)
{
	UNUSED(inode);
	return -ENOENT;
}

/*
 * Write an inode.
 */
int devfs_write_inode(struct inode_t *inode)
{
	/* nothing to do */
	UNUSED(inode);
	return 0;
}

/*
 * Put an inode.
 */
int devfs_put_inode(struct inode_t *inode)
{
	/* check inode */
	if (!inode)
		return -EINVAL;

	/* free inode */
	if (inode->i_ref <= 1 && !inode->i_nlinks) {
		inode->i_size = 0;
		clear_inode(inode);
		return 0;
	}

	/* inodes must not be released, since they are only in memory */
	if (!inode->i_ref)
		inode->i_ref = 1;

	return 0;
}

/*
 * Get a devfs inode.
 */
struct inode_t *devfs_iget(struct super_block_t *sb, struct devfs_entry_t *de)
{
	struct inode_t *inode, *tmp;

	/* try to find inode in global table */
	inode = find_inode(sb, de->ino);
	if (inode) {
		/* cross mount point */
		if (inode->i_mount) {
			tmp = inode->i_mount;
			tmp->i_ref++;
			iput(inode);
			inode = tmp;
		}

		return inode;
	}

	/* get an empty new inode */
	inode = get_empty_inode(sb);
	if (!inode)
		return NULL;

	/* set inode */
	inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;
	inode->i_ino = de->ino;
	inode->i_ref = 1;
	inode->i_nlinks = 1;
	inode->i_sb = sb;
	inode->i_mode = de->mode;
	inode->i_uid = current_task->uid;
	inode->i_gid = current_task->gid;
	inode->i_rdev = 0;
	inode->u.generic_i = de;

	/* set device */
	if (S_ISCHR(inode->i_mode) || S_ISBLK(inode->i_mode))
		inode->i_rdev = de->u.dev.dev;

	/* set number of links */
	if (S_ISDIR(inode->i_mode))
		inode->i_nlinks = 2;

	/* set operations */
	if (S_ISDIR(inode->i_mode))
		inode->i_op = &devfs_dir_iops;
	else if (S_ISLNK(inode->i_mode))
		inode->i_op = &devfs_symlink_iops;
	else if (S_ISCHR(inode->i_mode))
		inode->i_op = char_get_driver(inode);
	else if (S_ISBLK(inode->i_mode))
		inode->i_op = block_get_driver(inode);
	else
		goto err;

	/* hash inode */
	insert_inode_hash(inode);

	return inode;
err:
	inode->i_nlinks = 0;
	iput(inode);
	return NULL;
}