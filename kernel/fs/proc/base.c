#include <fs/proc_fs.h>
#include <proc/sched.h>
#include <string.h>
#include <stderr.h>
#include <fcntl.h>
#include <stdio.h>

#define PROC_MAXPIDS			20
#define PROC_NUMBUF			10

#define PROC_PID_INO			2
#define PROC_PID_STAT			3
#define PROC_PID_STATUS			4
#define PROC_PID_STATM			5
#define PROC_PID_CMDLINE		6
#define PROC_PID_ENVIRON		7
#define PROC_PID_FD			8

#define fake_ino(pid, ino)		(((pid) << 16) | (ino))

/*
 * Entry in <pid> directory.
 */
struct pid_entry {
	int		type;
	size_t		len;
	char *		name;
	mode_t		mode;
};

/*
 * Base entries.
 */
static struct pid_entry base_entries[] = {
	{ PROC_PID_STAT,	4,	"stat",		S_IFREG | S_IRUGO		},
	{ PROC_PID_STATUS,	6,	"status",	S_IFREG | S_IRUGO		},
	{ PROC_PID_STATM,	5,	"statm",	S_IFREG | S_IRUGO		},
	{ PROC_PID_CMDLINE,	7,	"cmdline",	S_IFREG | S_IRUGO		},
	{ PROC_PID_ENVIRON,	7,	"environ",	S_IFREG | S_IRUGO		},
	{ PROC_PID_FD,		2,	"fd",		S_IFDIR | S_IRUSR | S_IXUSR	},
	{ 0,			0,	NULL,		0				},
};

/*
 * Make a pid inode.
 */
static struct inode *proc_pid_make_inode(struct super_block *sb, struct task *task, ino_t ino)
{
	struct inode *inode;

	/* check task */
	if (!task || !task->pid)
		return NULL;

	/* get an inode */
	inode = get_empty_inode(sb);
	if (!inode)
		return NULL;

	/* set inode */
	inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME;
	inode->i_ino = fake_ino(task->pid, ino);
	inode->u.proc_i.task = task;
	inode->i_uid = 0;
	inode->i_gid = 0;
	if (ino == PROC_PID_INO) {
		inode->i_uid = task->euid;
		inode->i_gid = task->egid;
	}

	return inode;
}

/*
 * Revalidate a file descriptor dentry.
 */
static int pid_fd_revalidate(struct dentry *dentry)
{
	UNUSED(dentry);
	return 0;
}

/*
 * Revalidate a base dentry.
 */
static int pid_base_revalidate(struct dentry *dentry)
{
	if (dentry->d_inode->u.proc_i.task->pid)
		return 1;

	d_drop(dentry);
	return 0;
}

/*
 * Always delete pid dentries.
 */
static int pid_delete_dentry(struct dentry *dentry)
{
	UNUSED(dentry);
	return 1;
}

/*
 * File descriptor dentry operations.
 */
static struct dentry_operations pid_fd_dentry_operations = {
	.d_revalidate		= pid_fd_revalidate,
	.d_delete 		= pid_delete_dentry,
};

/*
 * Pid dentry operations.
 */
static struct dentry_operations pid_dentry_operations = {
	.d_delete 		= pid_delete_dentry,
};

/*
 * Base dentry operations.
 */
static struct dentry_operations pid_base_dentry_operations = {
	.d_revalidate		= pid_base_revalidate,
	.d_delete		= pid_delete_dentry,
};

/*
 * Follow a file descriptor link.
 */
static struct dentry *proc_fd_follow_link(struct dentry *dentry, struct dentry *base)
{
	struct inode *inode = dentry->d_inode;
	int ret;

	/* release base */
	dput(base);

	/* check permissions */
	ret = permission(inode, MAY_EXEC);
	if (ret)
		return ERR_PTR(ret);

	/* get file */
	return dget(inode->u.proc_i.filp->f_dentry);
}

/*
 * Read a file descriptor link.
 */
static ssize_t proc_fd_readlink(struct dentry *dentry, char *buf, size_t bufsize)
{
	/* follow link */
	dentry = proc_fd_follow_link(dentry, NULL);
	if (IS_ERR(dentry))
		return PTR_ERR(dentry);

	if (!dentry)
		return -ENOENT;

	/* read link */
	return do_proc_readlink(dentry, buf, bufsize);
}

/*
 * link inode operations.
 */
static struct inode_operations proc_fd_link_iops = {
	.readlink		= proc_fd_readlink,
	.follow_link		= proc_fd_follow_link,
};

/*
 * Read directory.
 */
static int proc_fd_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	struct inode *inode = filp->f_dentry->d_inode;
	struct task *task = inode->u.proc_i.task;
	size_t name_len, i;
	char fd_s[16];
	ino_t ino;
	int fd;

	/* add "." entry */
	if (filp->f_pos == 0) {
		if (filldir(dirent, ".", 1, filp->f_pos, inode->i_ino))
			return 0;
		filp->f_pos++;
	}

	/* add ".." entry */
	if (filp->f_pos == 1) {
		ino = fake_ino(task->pid, PROC_PID_INO);
		if (filldir(dirent, "..", 2, filp->f_pos, ino))
			return 0;
		filp->f_pos++;
	}

	/* add all files descriptors */
	for (fd = 0, i = 2; fd < NR_OPEN; fd++) {
		/* skip empty slots */
		if (!task->files->filp[fd])
			continue;

		/* skip files before offset */
		if (filp->f_pos > i++)
			continue;

		/* fill in directory entry */ 
		ino = fake_ino(task->pid, PROC_PID_FD_DIR + fd);
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
	struct task *task = dir->u.proc_i.task;
	struct inode *inode;
	struct file *filp;
	int fd;

	/* try to find matching file descriptor */
	fd = atoi(dentry->d_name.name);
	if (fd < 0 || fd >= NR_OPEN || !task->files->filp[fd])
		return ERR_PTR(-ENOENT);
	filp = task->files->filp[fd];

	/* make inode */
	inode = proc_pid_make_inode(dir->i_sb, task, PROC_PID_FD_DIR + fd);
	if (!inode)
		return ERR_PTR(-ENOENT);

	/* set inode */
	inode->i_op = &proc_fd_link_iops;
	inode->i_size = 64;
	inode->i_mode = S_IFLNK;
	inode->u.proc_i.filp = filp;
	if (filp->f_mode & 1)
		inode->i_mode |= S_IRUSR | S_IXUSR;
	if (filp->f_mode & 2)
		inode->i_mode |= S_IWUSR | S_IXUSR;

	dentry->d_op = &pid_fd_dentry_operations;
	d_add(dentry, inode);
	return NULL;
}

/*
 * File descriptors operations.
 */
static struct file_operations proc_fd_fops = {
	.readdir		= proc_fd_readdir,
};

/*
 * File descriptors inode operations.
 */
static struct inode_operations proc_fd_iops = {
	.fops			= &proc_fd_fops,
	.lookup			= proc_fd_lookup,
};

/*
 * Read base.
 */
static int proc_base_read(struct file *filp, char *buf, size_t count, off_t *ppos)
{
	struct inode *inode = filp->f_dentry->d_inode;
	struct task *task = inode->u.proc_i.task;
	size_t len;
	char *page;

	/* get a free page */
	page = get_free_page();
	if (!page)
		return -ENOMEM;

	/* read informations */
	len = inode->u.proc_i.proc_read(task, page);

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
 * Base file inode operations.
 */
static struct file_operations proc_base_file_fops = {
	.read			= proc_base_read,
};

/*
 * Base file inode operations.
 */
static struct inode_operations proc_base_file_iops = {
	.fops			= &proc_base_file_fops,
};

/*
 * Read <pid> directory.
 */
static int proc_base_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	struct inode *inode = filp->f_dentry->d_inode;
	size_t i = filp->f_pos;
	struct pid_entry *pe;
	pid_t pid;

	/* get task */
	pid = inode->u.proc_i.task->pid;
	if (!pid)
		return -ENOENT;

	/* "." entry */
	if (i == 0) {
		if (filldir(dirent, ".", 1, i, inode->i_ino) < 0)
			return 0;
		i++;
		filp->f_pos++;
	}

	/* ".." entry */
	if (i == 1) {
		if (filldir(dirent, "..", 2, i, PROC_ROOT_INO) < 0)
			return 0;
		i++;
		filp->f_pos++;
	}


	/* add entries */
	i -= 2;
	if (i >= sizeof(base_entries) / sizeof(base_entries[0]))
		return 1;
	for (pe = &base_entries[i]; pe->name; pe++) {
		if (filldir(dirent, pe->name, pe->len, filp->f_pos, fake_ino(pid, pe->type)) < 0)
			return 0;
		filp->f_pos++;
	}

	return 0;
}

/*
 * Lookup in <pid> directory.
 */
static struct dentry *proc_base_lookup(struct inode *dir, struct dentry *dentry)
{
	struct task *task = dir->u.proc_i.task;
	struct pid_entry *pe;
	struct inode *inode;

	/* find pid entry */
	for (pe = base_entries; pe->name; pe++) {
		if (pe->len != dentry->d_name.len)
			continue;
		if (!memcmp(dentry->d_name.name, pe->name, pe->len))
			break;
	}

	/* no matching entry */
	if (!pe->name)
		return ERR_PTR(-ENOENT);

	/* make an inode */
	inode = proc_pid_make_inode(dir->i_sb, task, pe->type);
	if (!inode)
		return ERR_PTR(-EINVAL);

	/* set inode */
	inode->i_mode = pe->mode;
	switch(pe->type) {
		case PROC_PID_STAT:
			inode->i_op = &proc_base_file_iops;
			inode->u.proc_i.proc_read = proc_stat_read;
			break;
		case PROC_PID_STATUS:
			inode->i_op = &proc_base_file_iops;
			inode->u.proc_i.proc_read = proc_status_read;
			break;
		case PROC_PID_STATM:
			inode->i_op = &proc_base_file_iops;
			inode->u.proc_i.proc_read = proc_statm_read;
			break;
		case PROC_PID_CMDLINE:
			inode->i_op = &proc_base_file_iops;
			inode->u.proc_i.proc_read = proc_cmdline_read;
			break;
		case PROC_PID_ENVIRON:
			inode->i_op = &proc_base_file_iops;
			inode->u.proc_i.proc_read = proc_environ_read;
			break;
		case PROC_PID_FD:
			inode->i_nlinks = 2;
			inode->i_op = &proc_fd_iops;
			break;
		default:
			printf("procfs: unknown type (%d)",pe->type);
			iput(inode);
			return ERR_PTR(-EINVAL);
	}

	dentry->d_op = &pid_dentry_operations;
	d_add(dentry, inode);
	return NULL;
}

/*
 * Base file operations.
 */
static struct file_operations proc_base_fops = {
	.readdir		= proc_base_readdir,
};

/*
 * Base inode operations.
 */
static struct inode_operations proc_base_iops = {
	.fops			= &proc_base_fops,
	.lookup			= proc_base_lookup,
};

/*
 * Follow self link.
 */
static struct dentry *proc_self_follow_link(struct dentry *dentry, struct dentry *base)
{
	char tmp[30];

	/* unused dentry */
	UNUSED(dentry);

	sprintf(tmp, "%d", current_task->pid);
	return lookup_dentry(AT_FDCWD, base, tmp, 1);
}

/*
 * Read self link.
 */
static ssize_t proc_self_readlink(struct dentry *dentry, char *buf, size_t bufsize)
{
	char tmp[32];
	size_t len;

	/* unused dentry */
	UNUSED(dentry);

	/* set target link */
	len = sprintf(tmp, "%d", current_task->pid) + 1;
	if (bufsize < len)
		len = bufsize;

	/* copy target link */
	memcpy(buf, tmp, len);

	return len;
}

/*
 * Self inode operations.
 */
struct inode_operations proc_self_iops = {
	.readlink	= proc_self_readlink,
	.follow_link	= proc_self_follow_link,
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
		ino = fake_ino(0, PROC_PID_INO);
		if (filldir(dirent, "self", 4, filp->f_pos, ino) < 0)
			return 0;
		filp->f_pos++;
		nr++;
	}

	/* get pids list */
	nr_pids = get_pid_list(nr, pid_array);

	/* add pids entries */
	for (i = 0; i < nr_pids; i++) {
		pid = pid_array[i];
		ino = fake_ino(pid, PROC_PID_INO);
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

	/* "self" entry */
	if (dentry->d_name.len == 4 && memcmp(dentry->d_name.name, "self", 4) == 0) {
		/* get an inode */
		inode = get_empty_inode(dir->i_sb);
		if (!inode)
			return ERR_PTR(-ENOMEM);

		/* set inode */
		inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME;
		inode->i_ino = fake_ino(0, PROC_PID_INO);
		inode->i_mode = S_IFLNK | S_IRWXUGO;
		inode->i_uid = inode->i_gid = 0;
		inode->i_size = 64;
		inode->i_op = &proc_self_iops;
		d_add(dentry, inode);
		return NULL;
	}

	/* else try to find matching process */
	pid = atoi(dentry->d_name.name);
	task = find_task(pid);
	if (!pid || !task)
		return ERR_PTR(-ENOENT);

	/* get an inode */
	inode = proc_pid_make_inode(dir->i_sb, task, PROC_PID_INO);
	if (!inode)
		return ERR_PTR(-ENOENT);

	/* set inode */
	inode->i_mode = S_IFDIR | S_IRUGO | S_IXUGO;
	inode->i_op = &proc_base_iops;
	inode->i_nlinks = 3;

	dentry->d_op = &pid_base_dentry_operations;
	d_add(dentry, inode);
	return NULL;
}