#include <fs/proc_fs.h>
#include <proc/sched.h>
#include <stdio.h>
#include <fcntl.h>
#include <stderr.h>

/*
 * Get directory entries.
 */
static int proc_fd_getdents64(struct file *filp, void *dirp, size_t count)
{
	struct dirent64 *dirent = (struct dirent64 *) dirp;
	struct task *task;
	size_t name_len, i;
	int fd, n = 0, ret;
	char fd_s[16];
	pid_t pid;

	/* get pid */
	pid = filp->f_inode->i_ino >> 16;

	/* add "." entry */
	if (filp->f_pos == 0) {
		/* fill in directory entry */ 
		ret = filldir(dirent, ".", 1, filp->f_inode->i_ino, count);
		if (ret)
			return n;

		/* go to next entry */
		count -= dirent->d_reclen;
		n += dirent->d_reclen;
		dirent = (struct dirent64 *) ((void *) dirent + dirent->d_reclen);
		filp->f_pos++;
	}

	/* add ".." entry */
	if (filp->f_pos == 1) {
		/* fill in directory entry */ 
		ret = filldir(dirent, "..", 2, (filp->f_inode->i_ino & 0xFFFF0000) + PROC_PID_INO, count);
		if (ret)
			return n;

		/* go to next entry */
		count -= dirent->d_reclen;
		n += dirent->d_reclen;
		dirent = (struct dirent64 *) ((void *) dirent + dirent->d_reclen);
		filp->f_pos++;
	}

	/* get task */
	task = get_task(pid);
	if (!task)
		return n;

	/* add all files descriptors */
	for (fd = 0, i = 2; fd < NR_OPEN; fd++) {
		/* skip empty slots */
		if (!task->files->filp[fd])
			continue;

		/* skip files before offset */
		if (filp->f_pos > i++)
			continue;


		/* fill in directory entry */ 
		name_len = sprintf(fd_s, "%d", fd);
		ret = filldir(dirent, fd_s, name_len, (pid << 16) + (PROC_PID_FD_INO << 8) + fd, count);
		if (ret)
			return n;

		/* go to next dir entry */
		count -= dirent->d_reclen;
		n += dirent->d_reclen;
		dirent = (struct dirent64 *) ((void *) dirent + dirent->d_reclen);

		/* update file position */
		filp->f_pos++;
	}

	return n;
}

/*
 * Lookup for a file.
 */
static int proc_fd_lookup(struct inode *dir, const char *name, size_t name_len, struct inode **res_inode)
{
	struct task *task;
	ino_t ino;
	pid_t pid;
	int fd;

	/* dir must be a directory */
	if (!dir)
		return -ENOENT;
	if (!S_ISDIR(dir->i_mode)) {
		iput(dir);
		return -ENOENT;
	}

	/* get pid */
	pid = dir->i_ino >> 16;

	/* current directory */
	if (!name_len || (name_len == 1 && name[0] == '.')) {
		*res_inode = dir;
		return 0;
	}

	/* parent directory */
	if (name_len == 2 && name[0] == '.' && name[1] == '.') {
		   *res_inode = iget(dir->i_sb, (pid << 16) + PROC_PID_INO);
		   if (!*res_inode) {
			iput(dir);
			return -ENOENT;
		   }

		   iput(dir);
		   return 0;
	    }

	/* get task */
	task = find_task(pid);
	if (!task) {
		iput(dir);
		return -ENOENT;
	}

	/* try to find matching file descriptor */
	fd = atoi(name);
	if (fd < 0 || fd >= NR_OPEN || !task->files->filp[fd]) {
		iput(dir);
		return -ENOENT;
	}

	/* create a fake inode */
	ino = (pid << 16) + (PROC_PID_FD_INO << 8) + fd;

	/* get inode */
	*res_inode = iget(dir->i_sb, ino);
	if (!*res_inode) {
		iput(dir);
		return -EACCES;
	}

	iput(dir);
	return 0;
}

/*
 * Process file descriptors operations.
 */
struct file_operations proc_fd_fops = {
	.getdents64		= proc_fd_getdents64,
};

/*
 * Process file descriptors inode operations.
 */
struct inode_operations proc_fd_iops = {
	.fops			= &proc_fd_fops,
	.lookup			= proc_fd_lookup,
};

/*
 * Follow fd link.
 */
static int proc_fd_follow_link(struct inode *dir, struct inode *inode, int flags, mode_t mode, struct inode **res_inode)
{
	struct task *task;
	pid_t pid;
	int fd;

	/* unused dir/flags/mode */
	UNUSED(dir);
	UNUSED(flags);
	UNUSED(mode);

	/* get task */
	pid = inode->i_ino >> 16;
	task = find_task(pid);
	if (!task) {
		iput(inode);
		return -ENOENT;
	}

	/* get file descriptor */
	fd = inode->i_ino & 0xFF;
	if (fd >= 0 && fd < NR_OPEN && task->files->filp[fd])
		*res_inode = task->files->filp[fd]->f_inode;

	/* release link inode */
	iput(inode);

	/* no matching link */
	if (!*res_inode)
		return -ENOENT;

	(*res_inode)->i_count++;
	return 0;
}

/*
 * Read fd link.
 */
static ssize_t proc_fd_readlink(struct inode *inode, char *buf, size_t bufsize)
{
	struct task *task;
	struct file *filp;
	char tmp[32];
	size_t len;
	pid_t pid;
	int fd;

	/* get task */
	pid = inode->i_ino >> 16;
	task = find_task(pid);
	if (!task) {
		iput(inode);
		return -ENOENT;
	}

	/* get file */
	fd = inode->i_ino & 0xFF;
	if (fd < 0 || fd >= NR_OPEN || !task->files->filp[fd]) {
		iput(inode);
		return -ENOENT;
	}
	filp = task->files->filp[fd];

	/* use file path */
	if (filp->f_path) {
		len = strlen(filp->f_path) + 1;
		if (bufsize < len)
			len = bufsize;

		memcpy(buf, filp->f_path, len);
		goto out;
	}

	/* else concat <pid>:<fd> */
	len = sprintf(tmp, "%d:%d", pid, fd) + 1;
	if (bufsize < len)
		len = bufsize;

	/* copy target link */
	memcpy(buf, tmp, len);

 out:
	iput(inode);
	return len;
}

/*
 * Fd link inode operations.
 */
struct inode_operations proc_fd_link_iops = {
	.readlink	= proc_fd_readlink,
	.follow_link	= proc_fd_follow_link,
};