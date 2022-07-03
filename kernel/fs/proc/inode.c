#include <fs/proc_fs.h>
#include <proc/sched.h>
#include <stderr.h>
#include <fcntl.h>

/*
 * Read an inode.
 */
int proc_read_inode(struct inode_t *inode)
{
	/* set inode */
	inode->i_op = NULL;
	inode->i_mode = 0;
	inode->i_uid = 0;
	inode->i_gid = 0;
	inode->i_nlinks = 1;
	inode->i_size = 0;
	inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;

	/* root directory */
	if (inode->i_ino == PROC_ROOT_INO) {
		inode->i_mode = S_IFDIR | S_IRUSR | S_IRGRP | S_IROTH | S_IXUSR | S_IXGRP | S_IXOTH;
		inode->i_nlinks = 2;
		inode->i_op = &proc_root_iops;
	}

	return 0;
}

/*
 * Write an inode.
 */
int proc_write_inode(struct inode_t *inode)
{
	/* procfs is read only */
	inode->i_dirt = 0;
	return 0;
}

/*
 * Release an inode.
 */
int proc_put_inode(struct inode_t *inode)
{
	if (inode->i_nlinks)
		inode->i_size = 0;

	return 0;
}
