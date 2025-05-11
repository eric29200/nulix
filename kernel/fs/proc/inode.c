#include <fs/proc_fs.h>
#include <proc/sched.h>
#include <stderr.h>
#include <fcntl.h>

/*
 * Read an inode.
 */
int proc_read_inode(struct inode *inode)
{
	struct task *task;
	ino_t ino;
	pid_t pid;
	int fd;

	/* set inode */
	inode->i_op = NULL;
	inode->i_mode = 0;
	inode->i_uid = 0;
	inode->i_gid = 0;
	inode->i_nlinks = 1;
	inode->i_size = 0;
	inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;

	/* get inode number */
	ino = inode->i_ino;
	pid = ino >> 16;

	/* root directory */
	if (!pid) {
		switch (ino) {
			case PROC_UPTIME_INO:
				inode->i_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH;
				inode->i_op = &proc_array_iops;
				break;
			case PROC_FILESYSTEMS_INO:
				inode->i_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH;
				inode->i_op = &proc_array_iops;
				break;
			case PROC_MOUNTS_INO:
				inode->i_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH;
				inode->i_op = &proc_array_iops;
				break;
			case PROC_SELF_INO:
				inode->i_mode = S_IFLNK | S_IRUSR | S_IRGRP | S_IROTH | S_IXUSR | S_IXGRP | S_IXOTH;
				inode->i_op = &proc_self_iops;
				break;
			case PROC_KSTAT_INO:
				inode->i_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH;
				inode->i_op = &proc_array_iops;
				break;
			case PROC_MEMINFO_INO:
				inode->i_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH;
				inode->i_op = &proc_array_iops;
				break;
			case PROC_LOADAVG_INO:
				inode->i_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH;
				inode->i_op = &proc_array_iops;
				break;
			case PROC_NET_INO:
				inode->i_mode = S_IFDIR | S_IRUSR | S_IRGRP | S_IROTH | S_IXUSR | S_IXGRP | S_IXOTH;
				inode->i_nlinks = 2;
				inode->i_op = &proc_net_iops;
				break;
			case PROC_NET_DEV_INO:
				inode->i_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH;
				inode->i_op = &proc_net_dev_iops;
				break;
			default:
				inode->i_mode = S_IFDIR | S_IRUSR | S_IRGRP | S_IROTH | S_IXUSR | S_IXGRP | S_IXOTH;
				inode->i_nlinks = 2;
				inode->i_op = &proc_root_iops;
				break;
		}

		return 0;
	}

	/* processes directories */
	ino &= 0x0000FFFF;
	switch (ino) {
		case PROC_PID_INO:
			inode->i_mode = S_IFDIR | S_IRUSR | S_IRGRP | S_IROTH | S_IXUSR | S_IXGRP | S_IXOTH;
			inode->i_nlinks = 2;
			inode->i_op = &proc_base_iops;
			return 0;
		case PROC_PID_STAT_INO:
			inode->i_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH;
			inode->i_op = &proc_stat_iops;
			return 0;
		case PROC_PID_CMDLINE_INO:
			inode->i_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH;
			inode->i_op = &proc_cmdline_iops;
			return 0;
		case PROC_PID_ENVIRON_INO:
			inode->i_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH;
			inode->i_op = &proc_environ_iops;
			return 0;
		case PROC_PID_FD_INO:
			inode->i_mode = S_IFDIR | S_IRUSR | S_IRGRP | S_IROTH | S_IXUSR | S_IXGRP | S_IXOTH;
			inode->i_nlinks = 2;
			inode->i_op = &proc_fd_iops;
			return 0;
	}

	/* processes sub directories */
	switch (ino >> 8) {
		case PROC_PID_FD_INO:
			/* get task */
			task = find_task(pid);
			if (task) {
				/* get file descriptor */
				fd = ino & 0xFF;
				if (fd >= 0 && fd < NR_OPEN && task->files->filp[fd]) {
					inode->i_mode = S_IFLNK | S_IRUSR | S_IRGRP | S_IROTH | S_IXUSR | S_IXGRP | S_IXOTH;
					inode->i_op = &proc_fd_link_iops;
					return 0;
				}
			}

			return 0;
	}

	return 0;
}

/*
 * Write an inode.
 */
int proc_write_inode(struct inode *inode)
{
	/* procfs is read only */
	inode->i_dirt = 0;
	return 0;
}

/*
 * Release an inode.
 */
int proc_put_inode(struct inode *inode)
{
	if (!inode->i_count)
		clear_inode(inode);

	return 0;
}
