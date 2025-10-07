#include <fs/proc_fs.h>
#include <proc/sched.h>
#include <string.h>
#include <stderr.h>
#include <fcntl.h>
#include <stdio.h>

#define PROC_MAXPIDS			20
#define PROC_NUMBUF			10

/*
 * Read array.
 */
static int proc_base_read(struct file *filp, char *buf, size_t count, off_t *ppos)
{
	struct task *task;
	char *page;
	size_t len;
	ino_t ino;
	pid_t pid;

	/* get inode number */
	ino = filp->f_dentry->d_inode->i_ino & 0x0000FFFF;
	pid = filp->f_dentry->d_inode->i_ino >> 16;

	/* find task */
	task = find_task(pid);
	if (!task)
		return -EINVAL;

	/* get a free page */
	page = get_free_page();
	if (!page)
		return -ENOMEM;

	/* get informations */
	switch (ino) {
		case PROC_PID_STAT_INO:
			len = proc_stat_read(task, page);
			break;
		case PROC_PID_STATUS_INO:
			len = proc_status_read(task, page);
			break;
		case PROC_PID_STATM_INO:
			len = proc_statm_read(task, page);
			break;
		case PROC_PID_CMDLINE_INO:
			len = proc_cmdline_read(task, page);
			break;
		case PROC_PID_ENVIRON_INO:
			len = proc_environ_read(task, page);
			break;
		default:
			count = -ENOMEM;
			goto out;
	}

	/* file position after end */
	if (*ppos >= len) {
		count = 0;
		goto out;
	}

	/* update count */
	if (*ppos + count > len)
		count = len - *ppos;

	/* copy content to user buffer and update file position */
	memcpy(buf, page + *ppos, count);
	*ppos += count;
out:
	free_page(page);
	return count;
}

/*
 * Base file operations.
 */
struct file_operations proc_base_fops = {
	.read		= proc_base_read,
};

/*
 * Base inode operations.
 */
struct inode_operations proc_base_iops = {
	.fops		= &proc_base_fops,
};

/*
 * Process file operations.
 */
static struct file_operations proc_base_dir_fops = {
	.readdir		= proc_readdir,
};

/*
 * Process inode operations.
 */
static struct inode_operations proc_base_dir_iops = {
	.fops			= &proc_base_dir_fops,
	.lookup			= proc_lookup,
};

/*
 * Base entries.
 */
struct proc_dir_entry proc_pid = {
	PROC_PID_INO, 5, "<pid>", S_IFDIR | S_IRUGO | S_IXUGO, 2, 0, 0, 0,
	&proc_base_dir_iops, NULL, NULL, &proc_root, NULL
};
static struct proc_dir_entry proc_pid_stat = {
	PROC_PID_STAT_INO, 4, "stat", S_IFREG | S_IRUGO, 1, 0, 0, 0,
	&proc_base_iops, NULL, NULL, NULL, NULL
};
static struct proc_dir_entry proc_pid_status = {
	PROC_PID_STATUS_INO, 6, "status", S_IFREG | S_IRUGO, 1, 0, 0, 0,
	&proc_base_iops, NULL, NULL, NULL, NULL
};
static struct proc_dir_entry proc_pid_statm = {
	PROC_PID_STATM_INO, 5, "statm", S_IFREG | S_IRUGO, 1, 0, 0, 0,
	&proc_base_iops, NULL, NULL, NULL, NULL
};
static struct proc_dir_entry proc_pid_cmdline = {
	PROC_PID_CMDLINE_INO, 7, "cmdline", S_IFREG | S_IRUGO, 1, 0, 0, 0,
	&proc_base_iops, NULL, NULL, NULL, NULL
};
static struct proc_dir_entry proc_pid_environ = {
	PROC_PID_ENVIRON_INO, 7, "environ", S_IFREG | S_IRUGO, 1, 0, 0, 0,
	&proc_base_iops, NULL, NULL, NULL, NULL
};
static struct proc_dir_entry proc_pid_fd = {
	PROC_PID_FD_INO, 2, "fd", S_IFDIR | S_IRUSR | S_IXUSR, 2, 0, 0, 0,
	&proc_fd_iops, NULL, NULL, NULL, NULL
};


/*
 * Get pids list.
 */
static int get_pid_list(int index, ino_t *pids)
{
	struct list_head *pos;
	struct task *task;
	int nr_pids = 0;

	/* self entry */
	index--;

	list_for_each(pos, &tasks_list) {
		/* skip init task */
		task = list_entry(pos, struct task, list);
		if (!task || !task->pid)
			continue;

		/* skip first tasks */
		if (--index >= 0)
			continue;

		/* add task */
		pids[nr_pids++] = task->pid;
		if (nr_pids >= PROC_MAXPIDS)
			break;
	}

	return nr_pids;
}

/*
 * Read pids entries.
 */
int proc_pid_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	size_t nr_pids, nr = filp->f_pos - FIRST_PROCESS_ENTRY, i, j;
	ino_t pid_array[PROC_MAXPIDS];
	char buf[PROC_NUMBUF];
	pid_t pid;
	ino_t ino;

	/* add "self" entry */
	if (!nr) {
		if (filldir(dirent, "self", 4, filp->f_pos, PROC_SELF_INO) < 0)
			return 0;
		filp->f_pos++;
		nr++;
	}

	/* get pids list */
	nr_pids = get_pid_list(nr, pid_array);

	/* add pids entries */
	for (i = 0; i < nr_pids; i++) {
		pid = pid_array[i];
		ino = (pid << 16) + PROC_PID_INO;
		j = PROC_NUMBUF;

		do {
			buf[--j] = '0' + (pid % 10);
		} while (pid /= 10);

		if (filldir(dirent, buf + j, PROC_NUMBUF - j, filp->f_pos, ino) < 0)
			break;

		filp->f_pos++;
	}

	return 0;
}

/*
 * Lookup a pid entry.
 */
struct dentry *proc_pid_lookup(struct inode *dir, struct dentry *dentry)
{
	struct inode *inode = NULL;
	struct task *task;
	pid_t pid;
	ino_t ino;

	/* "self" entry */
	if (dentry->d_name.len == 4 && memcmp(dentry->d_name.name, "self", 4) == 0) {
		inode = proc_get_inode(dir->i_sb, PROC_SELF_INO, NULL);
		if (!inode)
			return ERR_PTR(-EACCES);
		inode->i_mode = S_IFLNK | S_IRWXUGO;
		inode->i_op = &proc_self_iops;
		d_add(dentry, inode);
		return NULL;
	}

	/* else try to find matching process */
	pid = atoi(dentry->d_name.name);
	task = find_task(pid);
	if (!pid || !task)
		return ERR_PTR(-ENOENT);

	/* get inode */
	ino = (task->pid << 16) + PROC_PID_INO;
	inode = proc_get_inode(dir->i_sb, ino, &proc_pid);
	if (!inode)
		return ERR_PTR(-EACCES);

	d_add(dentry, inode);
	return NULL;
}

/*
 * Init base entries.
 */
void proc_base_init()
{
	proc_register(&proc_pid, &proc_pid_stat);
	proc_register(&proc_pid, &proc_pid_status);
	proc_register(&proc_pid, &proc_pid_statm);
	proc_register(&proc_pid, &proc_pid_cmdline);
	proc_register(&proc_pid, &proc_pid_environ);
	proc_register(&proc_pid, &proc_pid_fd);
}