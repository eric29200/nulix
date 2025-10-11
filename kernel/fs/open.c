#include <fs/fs.h>
#include <mm/mm.h>
#include <drivers/char/tty.h>
#include <proc/sched.h>
#include <stdio.h>
#include <stderr.h>
#include <fcntl.h>
#include <string.h>

/*
 * Open system call.
 */
static int do_open(int dirfd, const char *pathname, int flags, mode_t mode)
{
	struct dentry *dentry;
	struct inode *inode;
	struct file *filp;
	int flag, fd, ret;

	/* find a free slot in current process */
	fd = get_unused_fd();
	if (fd < 0)
		return fd;

	/* get an empty file */
	filp = get_empty_filp();
	if (!filp)
		return -EINVAL;

	filp->f_flags = flag = flags;
	filp->f_mode = (flag + 1) & O_ACCMODE;
	if (filp->f_mode)
		flag++;
	if (flag & O_TRUNC)
		flag |= 2;

	/* open file */
	dentry = open_namei(dirfd, pathname, flag, mode);
	ret = PTR_ERR(dentry);
	if (IS_ERR(dentry))
		goto err_cleanup_file;

	/* get inode */
	inode = dentry->d_inode;

	/* check inode operations */
	ret = -EINVAL;
	if (!inode->i_op)
		goto err_cleanup_dentry;

	/* check permissions */
	if (filp->f_mode & FMODE_WRITE) {
		ret = permission(inode, MAY_WRITE);
		if (ret)
			goto err_cleanup_dentry;
	}

	/* set file */
	FD_CLR(fd, &current_task->files->close_on_exec);
	filp->f_dentry = dentry;
	filp->f_pos = 0;
	filp->f_op = inode->i_op->fops;

	/* specific open function */
	if (filp->f_op && filp->f_op->open) {
		ret = filp->f_op->open(inode, filp);
		if (ret)
			goto err_cleanup_dentry;
	}

	current_task->files->filp[fd] = filp;
	return fd;
err_cleanup_dentry:
	dput(dentry);
err_cleanup_file:
	memset(filp, 0, sizeof(struct file));
	return ret;
}

/*
 * Open system call.
 */
int sys_open(const char *pathname, int flags, mode_t mode)
{
	return do_open(AT_FDCWD, pathname, flags, mode);
}

/*
 * Creat system call.
 */
int sys_creat(const char *pathname, mode_t mode)
{
	return do_open(AT_FDCWD, pathname, O_CREAT | O_TRUNC, mode);
}

/*
 * Openat system call.
 */
int sys_openat(int dirfd, const char *pathname, int flags, mode_t mode)
{
	return do_open(dirfd, pathname, flags, mode);
}

/*
 * Close a file.
 */
int close_fp(struct file *filp)
{
	/* check file */
	if (filp->f_count == 0) {
		printf("VFS: Close: file count is 0\n");
		return 0;
	}

	fput(filp);
	return 0;
}

/*
 * Close system call.
 */
int sys_close(int fd)
{
	struct file *filp;

	/* get file */
	if (fd < 0 || fd >= NR_OPEN || (filp = current_task->files->filp[fd]) == NULL)
		return -EBADF;

	/* close file */
	FD_CLR(fd, &current_task->files->close_on_exec);
	current_task->files->filp[fd] = NULL;
	return close_fp(filp);
}

/*
 * Chmod system call.
 */
static int do_chmod(int dirfd, const char *pathname, mode_t mode)
{
	struct dentry *dentry;
	struct inode *inode;

	/* resolve path */
	dentry = namei(dirfd, pathname, 1);
	if (IS_ERR(dentry))
		return PTR_ERR(dentry);

	/* get inode */
	inode = dentry->d_inode;

	/* adjust mode */
	if (mode == (mode_t) - 1)
		mode = inode->i_mode;

	/* change mode */
	inode->i_mode = (mode & S_IALLUGO) | (inode->i_mode & ~S_IALLUGO);
	mark_inode_dirty(inode);

	dput(dentry);
	return 0;
}

/*
 * Chmod system call.
 */
int sys_chmod(const char *pathname, mode_t mode)
{
	return do_chmod(AT_FDCWD, pathname, mode);
}

/*
 * Fchmod system call.
 */
static int do_fchmod(int fd, mode_t mode)
{
	struct dentry *dentry;
	struct inode *inode;
	struct file *filp;
	int ret;

	/* get file */
	filp = fget(fd);
	if (!filp)
		return -EINVAL;

	/* get dentry */
	ret = -ENOENT;
	dentry = filp->f_dentry;
	if (!dentry)
		goto out;

	/* get inode */
	inode = dentry->d_inode;
	if (!inode)
		goto out;

	/* adjust mode */
	if (mode == (mode_t) - 1)
		mode = inode->i_mode;

	/* change mode */
	inode->i_mode = (mode & S_IALLUGO) | (inode->i_mode & ~S_IALLUGO);
	mark_inode_dirty(inode);
	ret = 0;
out:
	fput(filp);
	return ret;
}

/*
 * Fchmod system call.
 */
int sys_fchmod(int fd, mode_t mode)
{
	return do_fchmod(fd, mode);
}

/*
 * Fchmodat system call.
 */
int sys_fchmodat(int dirfd, const char *pathname, mode_t mode, unsigned int flags)
{
	UNUSED(flags);
	return do_chmod(dirfd, pathname, mode);
}

/*
 * Chown system call.
 */
static int do_chown(int dirfd, const char *pathname, uid_t owner, gid_t group, unsigned int flags)
{
	struct dentry *dentry;
	struct inode *inode;

	/* resolve path */
	dentry = namei(dirfd, pathname, flags & AT_SYMLINK_NO_FOLLOW ? 0 : 1);
	if (IS_ERR(dentry))
		return PTR_ERR(dentry);

	/* get inode */
	inode = dentry->d_inode;

	/* update inode */
	inode->i_uid = owner;
	inode->i_gid = group;
	mark_inode_dirty(inode);

	dput(dentry);
	return 0;
}

/*
 * Chown system call.
 */
int sys_chown(const char *pathname, uid_t owner, gid_t group)
{
	return do_chown(AT_FDCWD, pathname, owner, group, 0);
}

/*
 * Lchown system call.
 */
int sys_lchown(const char *pathname, uid_t owner, gid_t group)
{
	return do_chown(AT_FDCWD, pathname, owner, group, AT_SYMLINK_NO_FOLLOW);
}

/*
 * Fchown system call.
 */
static int do_fchown(int fd, uid_t owner, gid_t group)
{
	struct dentry *dentry;
	struct inode *inode;
	struct file *filp;
	int ret;

	/* get file */
	filp = fget(fd);
	if (!filp)
		return -EINVAL;

	/* get dentry */
	ret = -ENOENT;
	dentry = filp->f_dentry;
	if (!dentry)
		goto out;

	/* get inode */
	inode = dentry->d_inode;
	if (!inode)
		goto out;

	/* update inode */
	inode->i_uid = owner;
	inode->i_gid = group;
	mark_inode_dirty(inode);
	ret = 0;
out:
	fput(filp);
	return ret;
}

/*
 * Fchown system call.
 */
int sys_fchown(int fd, uid_t owner, gid_t group)
{
	return do_fchown(fd, owner, group);
}

/*
 * Fchownat system call.
 */
int sys_fchownat(int dirfd, const char *pathname, uid_t owner, gid_t group, unsigned int flags)
{
	return do_chown(dirfd, pathname, owner, group, flags);
}

/*
 * Utimensat system call.
 */
static int do_utimensat(int dirfd, const char *pathname, struct kernel_timeval *times, int flags)
{
	struct dentry *dentry;
	struct inode *inode;
	int ret;

	/* resolve path */
	dentry = namei(dirfd, pathname, flags & AT_SYMLINK_NO_FOLLOW ? 0 : 1);
	if (IS_ERR(dentry))
		return PTR_ERR(dentry);

	/* get inode */
	inode = dentry->d_inode;

	/* check permissions */
	ret = permission(inode, MAY_WRITE);
	if (ret)
		goto out;

	/* set time */
	if (times)
		inode->i_atime = inode->i_mtime = times[0].tv_sec;
	else
		inode->i_atime = inode->i_mtime = CURRENT_TIME;

	/* mark inode dirty */
	mark_inode_dirty(inode);
out:
	dput(dentry);
	return ret;
}

/*
 * Utimensat system call.
 */
int sys_utimensat(int dirfd, const char *pathname, struct timespec *times, int flags)
{
	struct kernel_timeval ktimes[2];

	/* convert times to kernel timevals */
	if (times) {
		timespec_to_kernel_timeval(&times[0], &ktimes[0]);
		timespec_to_kernel_timeval(&times[1], &ktimes[1]);
	}

	return do_utimensat(dirfd, pathname, times ? ktimes : NULL, flags);
}

/*
 * Chroot system call.
 */
int sys_chroot(const char *path)
{
	struct dentry *dentry;
	struct inode *inode;
	int ret;

	/* resolve path */
	dentry = namei(AT_FDCWD, path, 1);
	if (IS_ERR(dentry))
		return PTR_ERR(dentry);

	/* get inode */
	inode = dentry->d_inode;

	/* check if it's a directory */
	ret = -ENOTDIR;
	if (!S_ISDIR(inode->i_mode))
		goto out;

	/* release current root directory and change it */
	dput(current_task->fs->root);
	current_task->fs->root = dentry;
	ret = 0;
out:
	dput(dentry);
	return ret;
}

/*
 * Hangup a tty.
 */
int sys_vhangup()
{
	return 0;
}