#include <fs/fs.h>
#include <proc/sched.h>
#include <stderr.h>
#include <string.h>

#define ROUND_UP64(x)		(((x) + sizeof(uint64_t) - 1) & ~(sizeof(uint64_t) - 1))
#define NAME_OFFSET(de)		((int) ((de)->d_name - (char *) (de)))

/*
 * Getdents64 argument.
 */
struct getdents_callback64 {
	struct dirent64 *	current_dir;
	struct dirent64 *	previous;
	size_t			count;
	int			error;
};

/*
 * Fill in a directory entry.
 */
static int filldir64(void *__buf, const char *name, size_t name_len, off_t offset, ino_t ino)
{
	struct getdents_callback64 *buf = (struct getdents_callback64 *) __buf;
	struct dirent64 * dirent, d;
	size_t reclen = ROUND_UP64(NAME_OFFSET(dirent) + name_len + 1);

	/* check space */
	buf->error = -EINVAL;
	if (reclen > buf->count)
		return -EINVAL;

	/* copy previous directory entry */
	dirent = buf->previous;
	if (dirent) {
		d.d_off = offset;
		memcpy(&dirent->d_off, &d.d_off, sizeof(d.d_off));
	}

	/* set directory entry */
	dirent = buf->current_dir;
	buf->previous = dirent;
	memset(&d, 0, NAME_OFFSET(&d));
	d.d_inode = ino;
	d.d_reclen = reclen;
	d.d_type = 0;
	memcpy(dirent, &d, NAME_OFFSET(&d));
	memcpy(dirent->d_name, name, name_len);
	dirent->d_name[name_len] = 0;

	/* go to next directory entry */
	dirent = (void *) dirent + reclen;
	buf->current_dir = dirent;
	buf->count -= reclen;

	return 0;
}

/*
 * Get directory entries system call.
 */
int sys_getdents64(int fd, void *dirp, size_t count)
{
	struct getdents_callback64 buf;
	struct dirent64 *lastdirent, d;
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
	buf.current_dir = dirp;
	buf.previous = NULL;
	buf.count = count;
	buf.error = 0;
	ret = filp->f_op->readdir(filp, &buf, filldir64);
	if (ret < 0)
		goto out_fput;

	/* copy last directory entry */
	ret = buf.error;
	lastdirent = buf.previous;
	if (lastdirent) {
		d.d_off = filp->f_pos;
		memcpy(&lastdirent->d_off, &d.d_off, sizeof(d.d_off));
		ret = count - buf.count;
	}

out_fput:
	fput(filp);
out:
	return ret;
}
