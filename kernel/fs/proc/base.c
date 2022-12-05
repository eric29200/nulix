#include <fs/proc_fs.h>
#include <proc/sched.h>
#include <string.h>
#include <stderr.h>
#include <fcntl.h>
#include <stdio.h>

#define NR_BASE_DIRENTRY		(sizeof(base_dir) / sizeof(base_dir[0]))

/*
 * Base process directory.
 */
static struct proc_dir_entry_t base_dir[] = {
	{ PROC_PID_INO,		1,	"." },
	{ PROC_ROOT_INO,	2,	".." },
	{ PROC_PID_STAT_INO,	4,	"stat" },
	{ PROC_PID_CMDLINE_INO,	7,	"cmdline" },
	{ PROC_PID_ENVIRON_INO,	7,	"environ" },
	{ PROC_PID_FD_INO,	2,	"fd" },
};

/*
 * Process states.
 */
static char proc_states[] = {
	'R',				/* running */
	'S',				/* sleeping */
	'T',				/* stopped */
	'Z',				/* zombie */
};

/*
 * Read process stat.
 */
static int proc_stat_read(struct file_t *filp, char *buf, int count)
{
	struct task_t *task;
	char tmp_buf[1024];
	size_t len;
	pid_t pid;

	/* get process */
	pid = filp->f_inode->i_ino >> 16;
	task = find_task(pid);
	if (!task)
		return -EINVAL;

	/* print pid in temporary buffer */
	len = sprintf(tmp_buf,	"%d (%s) "						/* pid, name */
				"%c %d "						/* state, ppid */
				"0 0 "							/* pgrp, session */
				"%d "							/* tty */
				"0 0 "							/* tpgid, flags */
				"0 0 0 0 "						/* minflt, cminflt, majflt, cmajflt */
				"%d %d %d %d "						/* utime, stime, cutime, cstime */
				"0 0 "							/* priority, nice */
				"0 0 "							/* num_threads, itrealvalue */
				"%d "							/* starttime */
				"0 0 0 \n",						/* vsize, rss, rsslim */
				task->pid, task->name, proc_states[task->state - 1],
				task->parent ? task->parent->pid : task->pid,
				(int) task->tty,
				task->utime, task->stime, task->cutime, task->cstime,
				task->start_time);

	/* file position after end */
	if (filp->f_pos >= len)
		return 0;

	/* update count */
	if (filp->f_pos + count > len)
		count = len - filp->f_pos;

	/* copy content to user buffer and update file position */
	memcpy(buf, tmp_buf + filp->f_pos, count);
	filp->f_pos += count;

	return count;
}

/*
 * Stat file operations.
 */
struct file_operations_t proc_stat_fops = {
	.read		= proc_stat_read,
};

/*
 * Stat inode operations.
 */
struct inode_operations_t proc_stat_iops = {
	.fops		= &proc_stat_fops,
};

/*
 * Read process command line.
 */
static int proc_cmdline_read(struct file_t *filp, char *buf, int count)
{
	char tmp_buf[PAGE_SIZE], *p, *arg_str;
	struct task_t *task;
	uint32_t arg;
	size_t len;
	pid_t pid;

	/* get process */
	pid = filp->f_inode->i_ino >> 16;
	task = find_task(pid);
	if (!task)
		return -EINVAL;

	/* switch to task's pgd */
	switch_page_directory(task->mm->pgd);

	/* get arguments */
	for (arg = task->mm->arg_start, p = tmp_buf; arg != task->mm->arg_end; arg += sizeof(char *)) {
		arg_str = *((char **) arg);

		/* copy argument */
		while (*arg_str && p - tmp_buf < PAGE_SIZE)
			*p++ = *arg_str++;

		/* overflow */
		if (p - tmp_buf >= PAGE_SIZE)
			break;

		/* end argument */
		*p++ = 0;
	}

	/* switch back to current's pgd */
	switch_page_directory(current_task->mm->pgd);

	/* file position after end */
	len = p - tmp_buf;
	if (filp->f_pos >= len)
		return 0;

	/* update count */
	if (filp->f_pos + count > len)
		count = len - filp->f_pos;

	/* copy content to user buffer and update file position */
	memcpy(buf, tmp_buf + filp->f_pos, count);
	filp->f_pos += count;

	return count;
}

/*
 * Cmdline file operations.
 */
struct file_operations_t proc_cmdline_fops = {
	.read		= proc_cmdline_read,
};

/*
 * Cmdline inode operations.
 */
struct inode_operations_t proc_cmdline_iops = {
	.fops		= &proc_cmdline_fops,
};

/*
 * Read process environ.
 */
static int proc_environ_read(struct file_t *filp, char *buf, int count)
{
	char tmp_buf[PAGE_SIZE], *p, *environ_str;
	struct task_t *task;
	uint32_t environ;
	size_t len;
	pid_t pid;

	/* get process */
	pid = filp->f_inode->i_ino >> 16;
	task = find_task(pid);
	if (!task)
		return -EINVAL;

	/* switch to task's pgd */
	switch_page_directory(task->mm->pgd);

	/* get environs */
	for (environ = task->mm->env_start, p = tmp_buf; environ != task->mm->env_end; environ += sizeof(char *)) {
		environ_str = *((char **) environ);

		/* copy environ */
		while (*environ_str && p - tmp_buf < PAGE_SIZE)
			*p++ = *environ_str++;

		/* overflow */
		if (p - tmp_buf >= PAGE_SIZE)
			break;

		/* end environ */
		*p++ = 0;
	}

	/* switch back to current's pgd */
	switch_page_directory(current_task->mm->pgd);

	/* file position after end */
	len = p - tmp_buf;
	if (filp->f_pos >= len)
		return 0;

	/* update count */
	if (filp->f_pos + count > len)
		count = len - filp->f_pos;

	/* copy content to user buffer and update file position */
	memcpy(buf, tmp_buf + filp->f_pos, count);
	filp->f_pos += count;

	return count;
}

/*
 * Environ file operations.
 */
struct file_operations_t proc_environ_fops = {
	.read		= proc_environ_read,
};

/*
 * Environ inode operations.
 */
struct inode_operations_t proc_environ_iops = {
	.fops		= &proc_environ_fops,
};

/*
 * Get directory entries.
 */
static int proc_fd_getdents64(struct file_t *filp, void *dirp, size_t count)
{
	struct dirent64_t *dirent = (struct dirent64_t *) dirp;
	struct task_t *task;
	size_t name_len, i;
	int fd, n = 0;
	char fd_s[16];
	pid_t pid;

	/* get pid */
	pid = filp->f_inode->i_ino >> 16;

	/* add "." entry */
	if (filp->f_pos == 0) {
		if (count < sizeof(struct dirent64_t) + 1 + 1)
			return n;

		dirent->d_inode = filp->f_inode->i_ino;
		dirent->d_type = 0;
		memcpy(dirent->d_name, ".", 1);
		dirent->d_name[1] = 0;
		dirent->d_reclen = sizeof(struct dirent64_t) + 1 + 1;
		count -= dirent->d_reclen;
		n += dirent->d_reclen;
		dirent = (struct dirent64_t *) ((void *) dirent + dirent->d_reclen);
		filp->f_pos++;
	}

	/* add ".." entry */
	if (filp->f_pos == 1) {
		if (count < sizeof(struct dirent64_t) + 2 + 1)
			return n;

		dirent->d_inode = (filp->f_inode->i_ino & 0xFFFF0000) + PROC_PID_INO;
		dirent->d_type = 0;
		memcpy(dirent->d_name, "..", 2);
		dirent->d_name[2] = 0;
		dirent->d_reclen = sizeof(struct dirent64_t) + 2 + 1;
		count -= dirent->d_reclen;
		n += dirent->d_reclen;
		dirent = (struct dirent64_t *) ((void *) dirent + dirent->d_reclen);
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

		/* check buffer size */
		name_len = sprintf(fd_s, "%d", fd);
		if (count < sizeof(struct dirent64_t) + name_len + 1)
			return n;

		/* set dir entry */
		dirent->d_inode = (pid << 16) + (PROC_PID_FD_INO << 8) + fd;
		dirent->d_type = 0;
		memcpy(dirent->d_name, fd_s, name_len);
		dirent->d_name[name_len] = 0;

		/* set dir entry size */
		dirent->d_reclen = sizeof(struct dirent64_t) + name_len + 1;

		/* go to next dir entry */
		count -= dirent->d_reclen;
		n += dirent->d_reclen;
		dirent = (struct dirent64_t *) ((void *) dirent + dirent->d_reclen);

		/* update file position */
		filp->f_pos++;
	}

	return n;
}

/*
 * Lookup for a file.
 */
static int proc_fd_lookup(struct inode_t *dir, const char *name, size_t name_len, struct inode_t **res_inode)
{
	struct task_t *task;
	ino_t ino;
	pid_t pid;
	int fd;

	/* unused name length */
	UNUSED(name_len);

	/* dir must be a directory */
	if (!dir)
		return -ENOENT;
	if (!S_ISDIR(dir->i_mode)) {
		iput(dir);
		return -ENOENT;
	}

	/* get task */
	pid = dir->i_ino >> 16;
	task = find_task(pid);
	if (!task) {
		iput(dir);
		return -ENOENT;
	}

	/* try to find matching file descriptor */
	fd = atoi(name);
	if (fd < 0 || fd >= NR_OPEN || !current_task->files->filp[fd]) {
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
struct file_operations_t proc_fd_fops = {
	.getdents64		= proc_fd_getdents64,
};

/*
 * Process file descriptors inode operations.
 */
struct inode_operations_t proc_fd_iops = {
	.fops			= &proc_fd_fops,
	.lookup			= proc_fd_lookup,
};

/*
 * Follow fd link.
 */
static int proc_fd_follow_link(struct inode_t *dir, struct inode_t *inode, int flags, mode_t mode, struct inode_t **res_inode)
{
	struct task_t *task;
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

	(*res_inode)->i_ref++;
	return 0;
}

/*
 * Read fd link.
 */
static ssize_t proc_fd_readlink(struct inode_t *inode, char *buf, size_t bufsize)
{
	char tmp[32];
	size_t len;

	/* set target link */
	len = sprintf(tmp, "%d:%d", inode->i_ino >> 16, inode->i_ino & 0xFF) + 1;
	if (bufsize < len)
		len = bufsize;

	/* release inode */
	iput(inode);

	/* copy target link */
	memcpy(buf, tmp, len);

	return len;
}

/*
 * Fd link inode operations.
 */
struct inode_operations_t proc_fd_link_iops = {
	.readlink	= proc_fd_readlink,
	.follow_link	= proc_fd_follow_link,
};

/*
 * Get directory entries.
 */
static int proc_base_getdents64(struct file_t *filp, void *dirp, size_t count)
{
	struct dirent64_t *dirent;
	int name_len, n;
	pid_t pid;
	size_t i;

	/* get pid */
	pid = filp->f_inode->i_ino >> 16;

	/* read root dir entries */
	for (i = filp->f_pos, n = 0, dirent = (struct dirent64_t *) dirp; i < NR_BASE_DIRENTRY; i++, filp->f_pos++) {
		/* check buffer size */
		name_len = base_dir[i].name_len;
		if (count < sizeof(struct dirent64_t) + name_len + 1)
			return n;

		/* set dir entry */
		dirent->d_inode = (pid << 16) + base_dir[i].ino;
		dirent->d_type = 0;
		memcpy(dirent->d_name, base_dir[i].name, name_len);
		dirent->d_name[name_len] = 0;

		/* set dir entry size */
		dirent->d_reclen = sizeof(struct dirent64_t) + name_len + 1;

		/* go to next dir entry */
		count -= dirent->d_reclen;
		n += dirent->d_reclen;
		dirent = (struct dirent64_t *) ((void *) dirent + dirent->d_reclen);
	}

	return n;
}

/*
 * Lookup for a file.
 */
static int proc_base_lookup(struct inode_t *dir, const char *name, size_t name_len, struct inode_t **res_inode)
{
	struct proc_dir_entry_t *de;
	ino_t ino;
	size_t i;

	/* dir must be a directory */
	if (!dir)
		return -ENOENT;
	if (!S_ISDIR(dir->i_mode)) {
		iput(dir);
		return -ENOENT;
	}

	/* find matching entry */
	for (i = 0, de = NULL; i < NR_BASE_DIRENTRY; i++) {
		if (proc_match(name, name_len, &base_dir[i])) {
			de = &base_dir[i];
			break;
		}
	}

	/* no such entry */
	if (!de) {
		iput(dir);
		return -ENOENT;
	}

	/* create a fake inode */
	ino = dir->i_ino - PROC_PID_INO + de->ino;

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
 * Process file operations.
 */
struct file_operations_t proc_base_fops = {
	.getdents64		= proc_base_getdents64,
};

/*
 * Process inode operations.
 */
struct inode_operations_t proc_base_iops = {
	.fops			= &proc_base_fops,
	.lookup			= proc_base_lookup,
};

