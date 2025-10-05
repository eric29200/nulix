#include <fs/proc_fs.h>
#include <proc/sched.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stderr.h>

/*
 * Follow a link.
 */
static struct dentry *proc_follow_link(struct dentry *dentry, struct dentry *base)
{
	struct inode *inode = dentry->d_inode;
	struct task *task;
	int ret, fd;
	ino_t ino;
	pid_t pid;

	/* release base */
	dput(base);

	/* check permissions */
	ret = permission(inode, MAY_EXEC);
	if (ret)
		return ERR_PTR(ret);

	/* get pid */
	ino = inode->i_ino;
	pid = ino >> 16;
	ino &= 0x0000FFFF;

	/* find task */
	task = find_task(pid);
	if (!task)
		return ERR_PTR(-ENOENT);
		 
	/* link = only files in /<pid>/fd/ */
	if (!(ino & PROC_PID_FD_DIR))
		return ERR_PTR(-ENOENT);

	/* get file descriptor */
	fd = ino & 0x7FFF;
	if (fd < 0 || fd >= NR_OPEN || !task->files->filp[fd])
		return ERR_PTR(-ENOENT);

	return dget(task->files->filp[fd]->f_dentry);
}

/*
 * Read a link.
 */
static int do_proc_readlink(struct dentry *dentry, char *buf, size_t bufsize)
{
	struct inode *inode = dentry->d_inode;
	char *page, *pattern = NULL, *path;
	size_t len;
	int err;

	/* get a free page */
	page = get_free_page();
	if (!page)
		return -ENOMEM;

	/* special dentries */
	if (inode && dentry->d_parent == dentry) {
		if (S_ISSOCK(inode->i_mode))
			pattern = "socket:[%lu]";
		if (S_ISFIFO(inode->i_mode))
			pattern = "pipe:[%lu]";
	}

	if (pattern) {
		len = sprintf(page, pattern, inode->i_ino);
		path = page;
	} else {
		path = d_path(dentry, page, PAGE_SIZE, &err);
		len = page + PAGE_SIZE - 1 - path;
	}

	if (bufsize < len)
		len = bufsize;

	/* copy result */
	memcpy(buf, path, len);

	dput(dentry);
	return len;
}

/*
 * Read a link.
 */
static ssize_t proc_readlink(struct dentry *dentry, char *buf, size_t bufsize)
{
	/* follow link */
	dentry = proc_follow_link(dentry, NULL);
	if (IS_ERR(dentry))
		return PTR_ERR(dentry);

	if (!dentry)
		return -ENOENT;

	/* do readlink */
	return do_proc_readlink(dentry, buf, bufsize);
}

/*
 * link inode operations.
 */
struct inode_operations proc_link_iops = {
	.readlink	= proc_readlink,
	.follow_link	= proc_follow_link,
};