#include <fs/fs.h>
#include <mm/mm.h>
#include <proc/sched.h>
#include <stderr.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>

/*
 * Stat64 system call.
 */
static int do_stat64(struct inode *inode, struct stat64 *statbuf)
{
	/* memzero statbuf */
	memset(statbuf, 0, sizeof(struct stat64));

	/* fill in statbuf */
	statbuf->__st_ino = inode->i_ino;
	statbuf->st_mode = inode->i_mode;
	statbuf->st_uid = inode->i_uid;
	statbuf->st_gid = inode->i_gid;
	statbuf->st_rdev = inode->i_rdev;
	statbuf->st_size = inode->i_size;
	statbuf->st_blksize = inode->i_sb->s_blocksize;
	statbuf->st_blocks = inode->i_blocks;
	statbuf->st_atime = inode->i_atime;
	statbuf->st_mtime = inode->i_mtime;
	statbuf->st_ctime = inode->i_ctime;
	statbuf->st_ino = inode->i_ino;

	return 0;
}

/*
 * Stat64 system call.
 */
int sys_stat64(const char *pathname, struct stat64 *statbuf)
{
	struct dentry *dentry;
	int ret;

	/* resolve path */
	dentry = namei(AT_FDCWD, NULL, pathname, 1);
	if (IS_ERR(dentry))
		return PTR_ERR(dentry);

	/* do stat */
	ret = do_stat64(dentry->d_inode, statbuf);

	dput(dentry);
	return ret;
}

/*
 * Lstat64 system call.
 */
int sys_lstat64(const char *pathname, struct stat64 *statbuf)
{
	struct dentry *dentry;
	int ret;

	/* resolve path */
	dentry = namei(AT_FDCWD, NULL, pathname, 0);
	if (IS_ERR(dentry))
		return PTR_ERR(dentry);

	/* do stat */
	ret = do_stat64(dentry->d_inode, statbuf);

	dput(dentry);
	return ret;
}

/*
 * Fstat64 system call.
 */
int sys_fstat64(int fd, struct stat64 *statbuf)
{
	struct file *filp;
	int ret;

	/* get file */
	filp = fget(fd);
	if (!filp)
		return -EINVAL;

	/* do stat */
	ret = do_stat64(filp->f_inode, statbuf);

	/* release file */
	fput(filp);

	return ret;
}

/*
 * Fstatat64 system call.
 */
int sys_fstatat64(int dirfd, const char *pathname, struct stat64 *statbuf, int flags)
{
	struct dentry *dentry;
	int ret;

	/* get inode */
	dentry = namei(dirfd, NULL, pathname, flags & AT_SYMLINK_NO_FOLLOW ? 0 : 1);
	if (IS_ERR(dentry))
		return PTR_ERR(dentry);

	/* do stat */
	ret = do_stat64(dentry->d_inode, statbuf);

	dput(dentry);
	return ret;
}

/*
 * Statx system call.
 */
int sys_statx(int dirfd, const char *pathname, int flags, unsigned int mask, struct statx *statbuf)
{
	struct dentry *dentry;
	struct inode *inode;

	/* unused mask */
	UNUSED(mask);

	/* resolve path */
	dentry = namei(dirfd, NULL, pathname, flags & AT_SYMLINK_NO_FOLLOW ? 0 : 1);
	if (IS_ERR(dentry))
		return PTR_ERR(dentry);

	/* get inode */
	inode = dentry->d_inode;

	/* reset stat buf */
	memset(statbuf, 0, sizeof(struct statx));

	/* set stat buf */
	statbuf->stx_mask |= STATX_BASIC_STATS;
	statbuf->stx_blksize = inode->i_sb->s_blocksize;
	statbuf->stx_nlink = inode->i_nlinks;
	statbuf->stx_uid = inode->i_uid;
	statbuf->stx_gid = inode->i_gid;
	statbuf->stx_mode = inode->i_mode;
	statbuf->stx_ino = inode->i_ino;
	statbuf->stx_size = inode->i_size;
	statbuf->stx_blocks = inode->i_size / 512;
	statbuf->stx_atime.tv_sec = inode->i_atime;
	statbuf->stx_btime.tv_sec = inode->i_atime;
	statbuf->stx_ctime.tv_sec = inode->i_ctime;
	statbuf->stx_mtime.tv_sec = inode->i_mtime;

	/* set minor/major */
	if (S_ISCHR(inode->i_mode) || S_ISBLK(inode->i_mode)) {
		statbuf->stx_rdev_major = major(inode->i_rdev);
		statbuf->stx_rdev_minor = minor(inode->i_rdev);
	}

	dput(dentry);
	return 0;
}
