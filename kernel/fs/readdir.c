#include <fs/fs.h>
#include <proc/sched.h>
#include <stderr.h>
#include <string.h>

/*
 * Fill in a directory entry.
 */
int filldir(struct dirent64 *dirent, const char *name, size_t name_len, ino_t ino, size_t max_len)
{
	/* not enough space to fill in directory entry */
	if (max_len < sizeof(struct dirent64) + name_len + 1)
		return -EINVAL;

	/* fill in dirent */
	dirent->d_inode = ino;
	dirent->d_off = 0;
	dirent->d_type = 0;
	dirent->d_reclen = sizeof(struct dirent64) + name_len + 1;
	memcpy(dirent->d_name, name, name_len);
	dirent->d_name[name_len] = 0;

	return 0;
}

/*
 * Get directory entries system call.
 */

int sys_getdents64(int fd, void *dirp, size_t count)
{
	struct dentry *dentry;
	struct inode *inode;
	struct file *filp;
	int ret;

	/* get file */
	ret = -EBADF;
	filp = fget(fd);
	if (!filp)
		goto out;

	/* get dentry */
	dentry = filp->f_dentry;
	if (!dentry)
		goto out_fput;

	/* get inode */
	inode = dentry->d_inode;
	if (!inode)
		goto out_fput;

	/* readdir not implemented */
	ret = -ENOTDIR;
	if (!filp->f_op || !filp->f_op->readdir)
		goto out_fput;

	/* do readdir */
	ret = filp->f_op->readdir(filp, dirp, count);
out_fput:
	fput(filp);
out:
	return ret;
}
