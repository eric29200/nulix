#include <fs/fs.h>
#include <mm/mm.h>
#include <drivers/tty.h>
#include <proc/sched.h>
#include <stdio.h>
#include <stderr.h>
#include <fcntl.h>
#include <string.h>

/* global file table */
struct file_t filp_table[NR_FILE];

/*
 * Get an empty file.
 */
struct file_t *get_empty_filp()
{
	int i;

	for (i = 0; i < NR_FILE; i++)
		if (!filp_table[i].f_ref)
			break;

	if (i >= NR_FILE)
		return NULL;

	filp_table[i].f_ref = 1;
	return &filp_table[i];
}

/*
 * Open system call.
 */
int do_open(int dirfd, const char *pathname, int flags, mode_t mode)
{
	struct inode_t *inode;
	struct file_t *filp;
	int fd, ret;

	/* find a free slot in current process */
	for (fd = 0; fd < NR_OPEN; fd++)
		if (!current_task->files->filp[fd])
			break;

	/* no slots : exit */
	if (fd >= NR_OPEN)
		return -EINVAL;

	/* get an empty file */
	filp = get_empty_filp();
	if (!filp)
		return -EINVAL;

	/* open file */
	ret = open_namei(dirfd, pathname, flags, mode, &inode);
	if (ret != 0)
		return ret;

	/* set file */
	current_task->files->filp[fd] = filp;
	filp->f_mode = inode->i_mode;
	filp->f_inode = inode;
	filp->f_flags = flags;
	filp->f_pos = 0;
	filp->f_op = inode->i_op->fops;

	/* specific open function */
	if (filp->f_op && filp->f_op->open)
		filp->f_op->open(filp);

	return fd;
}

/*
 * Close system call.
 */
int do_close(int fd)
{
	struct file_t *filp;

	/* check file descriptor */
	if (fd < 0 || fd >= NR_OPEN || !current_task->files->filp[fd])
		return -EINVAL;

	/* release file if not used anymore */
	filp = current_task->files->filp[fd];
	filp->f_ref--;
	if (filp->f_ref <= 0) {
		/* specific close operation */
		if (filp->f_op && filp->f_op->close)
			filp->f_op->close(filp);

		/* release inode */
		iput(filp->f_inode);
		memset(filp, 0, sizeof(struct file_t));
	}

	current_task->files->filp[fd] = NULL;

	return 0;
}

/*
 * Chmod system call.
 */
int do_chmod(int dirfd, const char *pathname, mode_t mode)
{
	struct inode_t *inode;

	/* get inode */
	inode = namei(dirfd, NULL, pathname, 1);
	if (!inode)
		return -ENOSPC;

	/* adjust mode */
	if (mode == (mode_t) - 1)
		mode = inode->i_mode;

	/* change mode */
	inode->i_mode = (mode & S_IALLUGO) | (inode->i_mode & ~S_IALLUGO);
	inode->i_dirt = 1;
	iput(inode);

	return 0;
}

/*
 * Fchmod system call.
 */
int do_fchmod(int fd, mode_t mode)
{
	struct inode_t *inode;

	/* check file descriptor */
	if (fd < 0 || fd >= NR_OPEN || !current_task->files->filp[fd])
		return -EINVAL;

	/* get inode */
	inode = current_task->files->filp[fd]->f_inode;

	/* adjust mode */
	if (mode == (mode_t) - 1)
		mode = inode->i_mode;

	/* change mode */
	inode->i_mode = (mode & S_IALLUGO) | (inode->i_mode & ~S_IALLUGO);
	inode->i_dirt = 1;

	return 0;
}

/*
 * Chown system call.
 */
int do_chown(int dirfd, const char *pathname, uid_t owner, gid_t group, unsigned int flags)
{
	struct inode_t *inode;

	/* get inode */
	inode = namei(dirfd, NULL, pathname, flags & AT_SYMLINK_NO_FOLLOW ? 0 : 1);
	if (!inode)
		return -ENOSPC;

	/* update inode */
	inode->i_uid = owner;
	inode->i_gid = group;
	inode->i_dirt = 1;
	iput(inode);

	return 0;
}

/*
 * Fchown system call.
 */
int do_fchown(int fd, uid_t owner, gid_t group)
{
	struct inode_t *inode;

	/* check file descriptor */
	if (fd < 0 || fd >= NR_OPEN || !current_task->files->filp[fd])
		return -EINVAL;

	/* update inode */
	inode = current_task->files->filp[fd]->f_inode;
	inode->i_uid = owner;
	inode->i_gid = group;
	inode->i_dirt = 1;

	return 0;
}

/*
 * Utimensat system call.
 */
int do_utimensat(int dirfd, const char *pathname, struct timespec_t *times, int flags)
{
	struct inode_t *inode;

	/* get inode */
	inode = namei(dirfd, NULL, pathname, flags & AT_SYMLINK_NO_FOLLOW ? 0 : 1);
	if (!inode)
		return -ENOENT;

	/* set time */
	if (times)
		inode->i_atime = inode->i_mtime = times[0].tv_sec;
	else
		inode->i_atime = inode->i_mtime = CURRENT_TIME;

	/* mark inode dirty */
	inode->i_dirt = 1;

	/* release inode */
	iput(inode);

	return 0;
}

/*
 * Chroot system call.
 */
int do_chroot(const char *path)
{
	struct inode_t *inode;

	/* get inode */
	inode = namei(AT_FDCWD, NULL, path, 1);
	if (!inode)
		return -ENOENT;

	/* check if it's a directory */
	if (!S_ISDIR(inode->i_mode))
		return -ENOTDIR;

	/* release current root directory and change it */
	iput(current_task->fs->root);
	current_task->fs->root = inode;

	return 0;
}
