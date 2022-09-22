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

	switch (inode->i_ino) {
		case PROC_ROOT_INO :
			inode->i_mode = S_IFDIR | S_IRUSR | S_IRGRP | S_IROTH | S_IXUSR | S_IXGRP | S_IXOTH;
			inode->i_nlinks = 2;
			inode->i_op = &proc_root_iops;
			break;
		case PROC_UPTIME_INO:
			inode->i_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH;
			inode->i_op = &proc_uptime_iops;
			break;
		case PROC_FILESYSTEMS_INO:
			inode->i_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH;
			inode->i_op = &proc_filesystems_iops;
			break;
		case PROC_MOUNTS_INO:
			inode->i_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH;
			inode->i_op = &proc_mounts_iops;
			break;
		case PROC_SELF_INO:
			inode->i_mode = S_IFLNK | S_IRUSR | S_IRGRP | S_IROTH | S_IXUSR | S_IXGRP | S_IXOTH;
			inode->i_op = &proc_self_iops;
			break;
		case PROC_KSTAT_INO:
			inode->i_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH;
			inode->i_op = &proc_kstat_iops;
			break;
		case PROC_MEMINFO_INO:
			inode->i_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH;
			inode->i_op = &proc_meminfo_iops;
			break;
		case PROC_LOADAVG_INO:
			inode->i_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH;
			inode->i_op = &proc_loadavg_iops;
			break;
		default:
			if (inode->i_ino >= PROC_BASE_INO) {
				inode->i_mode = S_IFDIR | S_IRUSR | S_IRGRP | S_IROTH | S_IXUSR | S_IXGRP | S_IXOTH;
				inode->i_nlinks = 2;
				inode->i_op = &proc_base_iops;
			}

			break;
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
