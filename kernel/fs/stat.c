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
int do_stat64(struct inode_t *inode, struct stat64_t *statbuf)
{
	/* memzero statbuf */
	memset(statbuf, 0, sizeof(struct stat64_t));

	/* fill in statbuf */
	statbuf->__st_ino = inode->i_ino;
	statbuf->st_mode = inode->i_mode;
	statbuf->st_nlink = inode->i_nlinks;
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
 * Statx system call.
 */
int do_statx(int dirfd, const char *pathname, int flags, unsigned int mask, struct statx_t *statbuf)
{
	struct inode_t *inode;

	/* unused mask */
	UNUSED(mask);

	/* get inode */
	inode = namei(dirfd, NULL, pathname, flags & AT_SYMLINK_NO_FOLLOW ? 0 : 1);
	if (!inode)
		return -ENOENT;

	/* reset stat buf */
	memset(statbuf, 0, sizeof(struct statx_t));

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
