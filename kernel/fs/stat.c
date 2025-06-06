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
	struct inode *inode;
	int ret;

	/* get inode */
	ret = namei(AT_FDCWD, NULL, pathname, 1, &inode);
	if (ret)
		return ret;

	/* do stat */
	ret = do_stat64(inode, statbuf);

	/* release inode */
	iput(inode);
	return ret;
}

/*
 * Lstat64 system call.
 */
int sys_lstat64(const char *pathname, struct stat64 *statbuf)
{
	struct inode *inode;
	int ret;

	/* get inode */
	ret = namei(AT_FDCWD, NULL, pathname, 0, &inode);
	if (ret)
		return ret;

	/* do stat */
	ret = do_stat64(inode, statbuf);

	/* release inode */
	iput(inode);
	return ret;
}

/*
 * Fstat64 system call.
 */
int sys_fstat64(int fd, struct stat64 *statbuf)
{
	struct file *filp;

	/* get file */
	filp = fget(fd);
	if (!filp)
		return -EINVAL;

	/* do stat */
	return do_stat64(filp->f_inode, statbuf);
}

/*
 * Fstatat64 system call.
 */
int sys_fstatat64(int dirfd, const char *pathname, struct stat64 *statbuf, int flags)
{
	struct inode *inode;
	int ret;

	/* get inode */
	ret = namei(dirfd, NULL, pathname, flags & AT_SYMLINK_NO_FOLLOW ? 0 : 1, &inode);
	if (ret)
		return ret;

	/* do stat */
	ret = do_stat64(inode, statbuf);

	/* release inode */
	iput(inode);
	return ret;
}

/*
 * Statx system call.
 */
int sys_statx(int dirfd, const char *pathname, int flags, unsigned int mask, struct statx *statbuf)
{
	struct inode *inode;
	int ret;

	/* unused mask */
	UNUSED(mask);

	/* get inode */
	ret = namei(dirfd, NULL, pathname, flags & AT_SYMLINK_NO_FOLLOW ? 0 : 1, &inode);
	if (ret)
		return ret;

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

	/* release inode */
	iput(inode);
	return 0;
}
