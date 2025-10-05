#include <fs/proc_fs.h>
#include <proc/sched.h>
#include <stdio.h>
#include <fcntl.h>
#include <stderr.h>

/*
 * Read directory.
 */
static int proc_fd_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	size_t name_len, i;
	struct task *task;
	char fd_s[16];
	pid_t pid;
	ino_t ino;
	int fd;

	/* get pid */
	pid = filp->f_dentry->d_inode->i_ino >> 16;

	/* add "." entry */
	if (filp->f_pos == 0) {
		if (filldir(dirent, ".", 1, filp->f_pos, filp->f_dentry->d_inode->i_ino))
			return 0;
		filp->f_pos++;
	}

	/* add ".." entry */
	if (filp->f_pos == 1) {
		if (filldir(dirent, "..", 2, filp->f_pos, (filp->f_dentry->d_inode->i_ino & 0xFFFF0000) | PROC_PID_INO))
			return 0;
		filp->f_pos++;
	}

	/* get task */
	task = get_task(pid);
	if (!task)
		return 0;

	/* add all files descriptors */
	for (fd = 0, i = 2; fd < NR_OPEN; fd++) {
		/* skip empty slots */
		if (!task->files->filp[fd])
			continue;

		/* skip files before offset */
		if (filp->f_pos > i++)
			continue;

		/* fill in directory entry */ 
		ino = (pid << 16) + PROC_PID_FD_DIR + fd;
		name_len = sprintf(fd_s, "%d", fd);
		if (filldir(dirent, fd_s, name_len, filp->f_pos, ino))
			return 0;

		/* update file position */
		filp->f_pos++;
	}

	return 0;
}

/*
 * Lookup for a file.
 */
static struct dentry *proc_fd_lookup(struct inode *dir, struct dentry *dentry)
{
	struct inode *inode = NULL;
	struct task *task;
	ino_t ino;
	pid_t pid;
	int fd;

	/* get pid */
	pid = dir->i_ino >> 16;

	/* get task */
	task = find_task(pid);
	if (!task)
		return ERR_PTR(-ENOENT);

	/* try to find matching file descriptor */
	fd = atoi(dentry->d_name.name);
	if (fd < 0 || fd >= NR_OPEN || !task->files->filp[fd])
		return ERR_PTR(-ENOENT);

	/* create a fake inode */
	ino = (pid << 16) + PROC_PID_FD_DIR + fd;

	/* get inode */
	inode = proc_get_inode(dir->i_sb, ino, NULL);
	if (!inode)
		return ERR_PTR(-EACCES);

	d_add(dentry, inode);
	return NULL;
}

/*
 * Process file descriptors operations.
 */
struct file_operations proc_fd_fops = {
	.readdir		= proc_fd_readdir,
};

/*
 * Process file descriptors inode operations.
 */
struct inode_operations proc_fd_iops = {
	.fops			= &proc_fd_fops,
	.lookup			= proc_fd_lookup,
};