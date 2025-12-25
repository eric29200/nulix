#include <fs/proc_fs.h>
#include <proc/sched.h>
#include <string.h>
#include <stderr.h>
#include <fcntl.h>
#include <stdio.h>

#define PROC_MAXPIDS			20
#define PROC_NUMBUF			13

#define MAPS_LINE_SHIFT			12
#define MAPS_LINE_LENGTH		(1 << MAPS_LINE_SHIFT)

#define ARRAY_SIZE(arr)			(sizeof(arr) / sizeof((arr)[0]))

typedef struct dentry *instantiate_t(struct inode *, struct dentry *, struct task *, const void *);

static ino_t last_ino = 2;
#define get_next_ino()			((last_ino)++)

/*
 * Entry in <pid> directory.
 */
struct pid_entry {
	int				type;
	char *				name;
	size_t				len;
	mode_t				mode;
	struct inode_operations *	i_op;
	union proc_op			op;
};

#define NOD(NAME, MODE, I_OP, OP) {			\
	.name = (NAME),					\
	.len  = sizeof(NAME) - 1,			\
	.mode = MODE,					\
	.i_op = I_OP,					\
	.op   = OP,					\
}

#define REG(NAME, MODE, I_OP)		NOD(NAME, (S_IFREG | (MODE)), &I_OP, {})
#define INF(NAME, MODE, read)		NOD(NAME, (S_IFREG | (MODE)), &proc_base_file_iops, { .proc_read = read })
#define LNK(NAME, get_link)		NOD(NAME, (S_IFLNK | S_IRWXUGO), &proc_base_link_iops, { .proc_get_link = get_link })
#define DIR(NAME, MODE, I_OP)		NOD(NAME, (S_IFDIR | (MODE)), &I_OP, {})

static struct inode_operations proc_self_iops;
static struct inode_operations proc_base_file_iops;
static struct inode_operations proc_base_link_iops;
static struct inode_operations proc_base_maps_iops;
static struct inode_operations proc_fd_iops;
static struct inode_operations proc_task_iops;
static int proc_root_link(struct task *task, struct dentry **dentry);
static int proc_cwd_link(struct task *task, struct dentry **dentry);
static int proc_exe_link(struct task *task, struct dentry **dentry);

/*
 * root entries.
 */
static struct pid_entry proc_base_entries[] = {
	NOD("self",	S_IFLNK | S_IRWXUGO, &proc_self_iops, {}),
};

/*
 * <pid> entries.
 */
static struct pid_entry pid_base_entries[] = {
	INF("stat",	S_IRUGO,		proc_stat_read		),
	INF("status",	S_IRUGO,		proc_status_read	),
	INF("statm",	S_IRUGO,		proc_statm_read		),
	INF("cmdline",	S_IRUGO,		proc_cmdline_read	),
	INF("environ",	S_IRUGO,		proc_environ_read	),
	INF("io",	S_IRUGO,		proc_io_read		),
	INF("comm",	S_IRUGO,		proc_comm_read		),
	REG("maps",	S_IRUGO,		proc_base_maps_iops	),
	LNK("root",				proc_root_link		),
	LNK("cwd",				proc_cwd_link		),
	LNK("exe",				proc_exe_link		),
	DIR("fd",	S_IRUSR | S_IXUSR,	proc_fd_iops		),
	DIR("task",	S_IRUGO | S_IXUGO,	proc_task_iops		),
};

/*
 * <pid>/task/<tid> entries.
 */
static struct pid_entry tid_base_entries[] = {
	INF("stat",	S_IRUGO,		proc_stat_read		),
	INF("status",	S_IRUGO,		proc_status_read	),
	INF("statm",	S_IRUGO,		proc_statm_read		),
	INF("cmdline",	S_IRUGO,		proc_cmdline_read	),
	INF("environ",	S_IRUGO,		proc_environ_read	),
	INF("io",	S_IRUGO,		proc_io_read		),
	INF("comm",	S_IRUGO,		proc_comm_read		),
	REG("maps",	S_IRUGO,		proc_base_maps_iops	),
	LNK("root",				proc_root_link		),
	LNK("cwd",				proc_cwd_link		),
	LNK("exe",				proc_exe_link		),
	DIR("fd",	S_IRUSR | S_IXUSR,	proc_fd_iops		),
};

/*
 * Get task of a pid inode.
 */
static struct task *get_proc_task(struct inode *inode)
{
	return get_task(inode->u.proc_i.pid);
}

/*
 * Count number of directories.
 */
static size_t pid_entry_count_dirs(const struct pid_entry *entries, size_t n)
{
	size_t i, count = 0;

	for (i = 0; i < n; ++i)
		if (S_ISDIR(entries[i].mode))
			count++;

	return count;
}

/*
 * Make a pid inode.
 */
static struct inode *proc_pid_make_inode(struct super_block *sb, struct task *task)
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
	inode->i_ino = get_next_ino();
	inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME;
	inode->u.proc_i.pid = task->pid;
	inode->i_uid = task->euid;
	inode->i_gid = task->egid;

	return inode;
}

/*
 * Instantiate and cache a pid inode.
 */
static int proc_fill_cache(struct file *filp, void *dirent, filldir_t filldir, char *name, size_t len, instantiate_t instantiate, struct task *task, const void *ptr)
{
	struct dentry *child, *new, *dir = filp->f_dentry;
	struct inode *inode;
	struct qstr qname;
	ino_t ino = 0;

	/* hash name */
	qname.name = name;
	qname.len  = len;
	qname.hash = full_name_hash(name, len);

	/* lookup and create inode if needed */
	child = d_lookup(dir, &qname);
	if (!child) {
		new = d_alloc(dir, &qname);
		if (new) {
			child = instantiate(dir->d_inode, new, task, ptr);
			if (child)
				dput(new);
			else
				child = new;
		}
	}

	/* handle error */
	if (!child || IS_ERR(child) || !child->d_inode)
		goto out;

	/* get inode number */
	inode = child->d_inode;
	if (inode)
		ino = inode->i_ino;

	dput(child);
out:
	if (!ino)
		ino = find_inode_number(dir, &qname);
	if (!ino)
		ino = 1;
	return filldir(dirent, name, len, filp->f_pos, ino);
}

/*
 * Revalidate a pid dentry.
 */
static int pid_revalidate(struct dentry *dentry)
{
	struct inode *inode = dentry->d_inode;
	struct task *task;

	/* check task */
	task = get_proc_task(inode);
	if (!task) {
		d_drop(dentry);
		return 0;
	}

	/* update uid/gid */
	if (inode->i_mode == (S_IFDIR | S_IRUGO | S_IXUGO)) {
		inode->i_uid = task->euid;
		inode->i_gid = task->egid;
	} else {
		inode->i_uid = 0;
		inode->i_gid = 0;
	}

	inode->i_mode &= ~(S_ISUID | S_ISGID);
	return 1;
}

/*
 * Delete a pid dentry.
 */
static int pid_delete_dentry(struct dentry *dentry)
{
	if (get_proc_task(dentry->d_inode))
		return 0;

	d_drop(dentry);
	return 1;
}

/*
 * Pid dentry operations.
 */
static struct dentry_operations pid_dentry_operations = {
	.d_revalidate	= pid_revalidate,
	.d_delete	= pid_delete_dentry,
};

/*
 * Get root directory dentry.
 */
static int proc_root_link(struct task *task, struct dentry **dentry)
{
	struct fs_struct *fs = task->fs;

	if (!fs)
		return -ENOENT;

	*dentry = dget(fs->root);
	return 0;
}

/*
 * Get current working directory dentry.
 */
static int proc_cwd_link(struct task *task, struct dentry **dentry)
{
	struct fs_struct *fs = task->fs;

	if (!fs)
		return -ENOENT;

	*dentry = dget(fs->pwd);
	return 0;
}

/*
 * Get current executable dentry.
 */
static int proc_exe_link(struct task *task, struct dentry **dentry)
{
	struct list_head *pos;
	struct vm_area *vma;

	/* find first executable memory region */
	list_for_each(pos, &task->mm->vm_list) {
		vma = list_entry(pos, struct vm_area, list);
		if ((vma->vm_flags & VM_EXEC) && vma->vm_file)
			goto out;
	}

	return -ENOENT;
out:
	*dentry = dget(vma->vm_file->f_dentry);
	return 0;
}

/*
 * Read a link.
 */
static ssize_t proc_base_readlink(struct dentry *dentry, char *buf, size_t bufsize)
{
	struct inode *inode = dentry->d_inode;
	struct dentry *res;
	struct task *task;
	int ret;

	/* get task */
	task = get_proc_task(inode);
	if (!task)
		return -ESRCH;

	/* get link */
	ret = inode->u.proc_i.op.proc_get_link(task, &res);
	if (ret)
		return ret;

	/* read link */
	ret = do_proc_readlink(res, buf, bufsize);
	dput(res);

	return ret;
}

/*
 * Follow a link.
 */
static struct dentry *proc_base_follow_link(struct dentry *dentry, struct dentry *base)
{
	struct inode *inode = dentry->d_inode;
	struct dentry *res;
	struct task *task;
	int ret;

	/* release base */
	dput(base);

	/* check permissions */
	ret = permission(inode, MAY_EXEC);
	if (ret)
		return ERR_PTR(ret);

	/* get task */
	task = get_proc_task(inode);
	if (!task)
		return ERR_PTR(-ESRCH);

	/* get link */
	ret = inode->u.proc_i.op.proc_get_link(task, &res);
	if (ret)
		return ERR_PTR(ret);

	return res;
}

/*
 * Base link inode operations.
 */
static struct inode_operations proc_base_link_iops = {
	.readlink		= proc_base_readlink,
	.follow_link		= proc_base_follow_link,
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
static struct inode_operations proc_self_iops = {
	.readlink	= proc_self_readlink,
	.follow_link	= proc_self_follow_link,
};

/*
 * Read /<pid>/maps
 */
static ssize_t proc_base_maps_read(struct file *filp, char *buf, size_t count, off_t *ppos)
{
	struct inode *inode = filp->f_dentry->d_inode;
	char *page, *line, perms[5], *destptr = buf;
	ssize_t column, i = 0, len;
	size_t j, maxlen = 60;
	struct list_head *pos;
	struct vm_area *vma;
	struct task *task;
	off_t lineno;
	dev_t dev;
	ino_t ino;
	int err;

	/* get task */
	task = get_proc_task(inode);
	if (!task)
		return -ESRCH;

	if (!task->mm || count == 0)
		return 0;

	/* get a page */
	page = get_free_page();
	if (!page)
		return -ENOMEM;

	/* decode file position */
	lineno = *ppos >> MAPS_LINE_SHIFT;
	column = *ppos & (MAPS_LINE_LENGTH - 1);

	/* for each memory area */
	list_for_each(pos, &task->mm->vm_list) {
		/* skip first areas */
		if (i++ < lineno)
			continue;

		/* get memory area */
		vma = list_entry(pos, struct vm_area, list);

		/* get permissions */
		perms[0] = vma->vm_flags & VM_READ ? 'r' : '-';
		perms[1] = vma->vm_flags & VM_WRITE ? 'w' : '-';
		perms[2] = vma->vm_flags & VM_EXEC ? 'x' : '-';
		perms[3] = vma->vm_flags & VM_SHARED ? 's' : 'p';
		perms[4] = 0;

		/* get device and inode number */
		if (vma->vm_file) {
			dev = vma->vm_file->f_dentry->d_inode->i_dev;
			ino = vma->vm_file->f_dentry->d_inode->i_ino;
			line = d_path(vma->vm_file->f_dentry, page, PAGE_SIZE, &err);
			page[PAGE_SIZE - 1] = '\n';
			line -= maxlen;
			if (line < page)
				line = page;
		} else {
			dev = 0;
			ino = 0;
			line = page;
		}

		/* print memory area */
		len = sprintf(line, "%08lx-%08lx %s %016llx %02x:%02x %lu",
			vma->vm_start,
			vma->vm_end, perms,
			vma->vm_offset,
			(int) major(dev),
			(int) minor(dev),
			ino);

		/* print path */
		if (vma->vm_file) {
			for (j = len; j < maxlen; j++)
				line[j] = ' ';
			len = page + PAGE_SIZE - line;
		} else {
			line[len++] = '\n';
		}

		/* skip memory area */
		if (column >= len) {
			column = 0;
			lineno++;
			continue;
		}

		/* limit */
		j = len - column;
		if (j > count)
			j = count;

		/* copy to user buffer */
		memcpy(destptr, line + column, j);

		/* update sizes */
		destptr += j;
		count -= j;
		column += j;

		/* next time : line at column 0 */
		if (column >= len) {
			column = 0;
			lineno++;
		}

		/* done */
		if (!count)
			break;
	}

	/* encode file position */
	*ppos = (lineno << MAPS_LINE_SHIFT) + column;

	/* free page */
	free_page(page);

	return destptr - buf;
}

/*
 * Base maps file operations.
 */
static struct file_operations proc_base_maps_fops = {
	.read			= proc_base_maps_read,
};

/*
 * Base maps inode operations.
 */
static struct inode_operations proc_base_maps_iops = {
	.fops			= &proc_base_maps_fops,
};

/*
 * Read base.
 */
static int proc_base_read(struct file *filp, char *buf, size_t count, off_t *ppos)
{
	struct inode *inode = filp->f_dentry->d_inode;
	struct task *task;
	size_t len;
	char *page;

	/* get task */
	task = get_proc_task(inode);
	if (!task)
		return -ESRCH;

	/* get a free page */
	page = get_free_page();
	if (!page)
		return -ENOMEM;

	/* read informations */
	len = inode->u.proc_i.op.proc_read(task, page);

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
static struct file_operations proc_base_file_fops = {
	.read			= proc_base_read,
};

/*
 * Base inode operations.
 */
static struct inode_operations proc_base_file_iops = {
	.fops			= &proc_base_file_fops,
};

/*
 * Revalidate a fd inode.
 */
static int pid_fd_revalidate(struct dentry *dentry)
{
	struct inode *inode = dentry->d_inode;
	struct task *task;
	int fd;

	/* check task */
	task = get_proc_task(inode);
	if (!task) {
		d_drop(dentry);
		return 0;
	}

	/* check file */
	fd = inode->u.proc_i.fd;
	if (fd < 0 || fd >= NR_OPEN || !task->files->filp[fd]) {
		d_drop(dentry);
		return 0;
	}

	/* update inode */
	inode->i_uid = task->euid;
	inode->i_gid = task->egid;
	inode->i_mode &= ~(S_ISUID | S_ISGID);
	return 1;
}

/*
 * Fd dentry operations.
 */
struct dentry_operations pid_fd_dentry_operations =
{
	.d_revalidate	= pid_fd_revalidate,
	.d_delete	= pid_delete_dentry,
};

/*
 * Follow a file descriptor link.
 */
static struct dentry *proc_fd_follow_link(struct dentry *dentry, struct dentry *base)
{
	struct inode *inode = dentry->d_inode;
	struct task *task;
	struct file *filp;
	int ret, fd;

	/* release base */
	dput(base);

	/* check permissions */
	ret = permission(inode, MAY_EXEC);
	if (ret)
		return ERR_PTR(ret);

	/* get task */
	task = get_proc_task(inode);
	if (!task)
		return ERR_PTR(-ENOENT);

	/* get file */
	fd = inode->u.proc_i.fd;
	if (fd < 0 || fd >= NR_OPEN || !task->files->filp[fd])
		return ERR_PTR(-ENOENT);
	filp = task->files->filp[fd];

	/* get file */
	return dget(filp->f_dentry);
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
 * Instantiate a fd inode.
 */
static struct dentry *proc_fd_instantiate(struct inode *dir, struct dentry *dentry, struct task *task, const void *ptr)
{
	const int fd = *(const int *) ptr;
	struct proc_inode_info *ei;
	struct inode *inode;
	struct file *filp;

	/* get file */
	if (fd < 0 || fd >= NR_OPEN || !task->files->filp[fd])
		return ERR_PTR(-ENOENT);
	filp = task->files->filp[fd];

	/* make inode */
	inode = proc_pid_make_inode(dir->i_sb, task);
	if (!inode)
		return ERR_PTR(-ENOENT);

	/* set inode */
	ei = &inode->u.proc_i;
	inode->i_mode = S_IFLNK;
	if (filp->f_mode & FMODE_READ)
		inode->i_mode |= S_IRUSR | S_IXUSR;
	if (filp->f_mode & FMODE_WRITE)
		inode->i_mode |= S_IWUSR | S_IXUSR;
	inode->i_op = &proc_fd_link_iops;
	inode->i_size = 64;
	ei->fd = fd;

	/* set dentry */
	dentry->d_op = &pid_fd_dentry_operations;
	d_add(dentry, inode);

	return pid_fd_revalidate(dentry) ? NULL : ERR_PTR(-ENOENT);
}

/*
 * Read fd directory.
 */
static int proc_fd_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	struct dentry *dentry = filp->f_dentry;
	struct inode *inode = dentry->d_inode;
	char name[PROC_NUMBUF];
	struct task *task;
	size_t len, i;
	int fd;

	/* get task */
	task = get_proc_task(inode);
	if (!task)
		return -ENOENT;

	/* add "." entry */
	if (filp->f_pos == 0) {
		if (filldir(dirent, ".", 1, filp->f_pos, inode->i_ino))
			return 0;
		filp->f_pos++;
	}

	/* add ".." entry */
	if (filp->f_pos == 1) {
		if (filldir(dirent, "..", 2, filp->f_pos, dentry->d_parent->d_inode->i_ino))
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
		len = sprintf(name, "%d", fd);
		if (proc_fill_cache(filp, dirent, filldir, name, len, proc_fd_instantiate, task, &fd) < 0)
			return 0;

		/* update file position */
		filp->f_pos++;
	}

	return 0;
}

/*
 * Lookup fd directory.
 */
static struct dentry *proc_fd_lookup(struct inode *dir, struct dentry *dentry)
{
	struct task *task;
	int fd;

	/* get task */
	task = get_proc_task(dir);
	if (!task)
		return ERR_PTR(-ENOENT);

	/* parse file descriptor */
	fd = atoi(dentry->d_name.name);

	return proc_fd_instantiate(dir, dentry, task, &fd);
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
 * Instantiate a pid entry.
 */
static struct dentry *proc_pident_instantiate(struct inode *dir, struct dentry *dentry, struct task *task, const void *ptr)
{
	const struct pid_entry *p = ptr;
	struct proc_inode_info *ei;
	struct inode *inode;

	/* make inode */
	inode = proc_pid_make_inode(dir->i_sb, task);
	if (!inode)
		return ERR_PTR(-ENOENT);

	/* set inode */
	ei = &inode->u.proc_i;
	inode->i_mode = p->mode;
	if (S_ISDIR(inode->i_mode))
		inode->i_nlinks = 2;
	if (p->i_op)
		inode->i_op = p->i_op;
	ei->op = p->op;

	/* set dentry */
	dentry->d_op = &pid_dentry_operations;
	d_add(dentry, inode);

	return pid_revalidate(dentry) ? NULL : ERR_PTR(-ENOENT);
}

/*
 * Fill a pid entry.
 */
static int proc_pident_fill_cache(struct file *filp, void *dirent, filldir_t filldir, struct task *task, const struct pid_entry *p)
{
	return proc_fill_cache(filp, dirent, filldir, p->name, p->len, proc_pident_instantiate, task, p);
}

/*
 * Read a directory based on pid entries.
 */
static int proc_pident_readdir(struct file *filp, void *dirent, filldir_t filldir, const struct pid_entry *ents, size_t nents)
{
	struct dentry *dentry = filp->f_dentry;
	struct inode *inode = dentry->d_inode;
	const struct pid_entry *p;
	size_t i = filp->f_pos;
	struct task *task;

	/* get task */
	task = get_proc_task(inode);
	if (!task)
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
		if (filldir(dirent, "..", 2, i, dentry->d_parent->d_inode->i_ino) < 0)
			return 0;
		i++;
		filp->f_pos++;
	}

	/* add entries */
	i -= 2;
	if (i >= nents)
		return 1;
	for (; i < nents; i++, filp->f_pos++) {
		p = &ents[i];
		if (proc_pident_fill_cache(filp, dirent, filldir, task, p) < 0)
			return 0;
	}

	return 0;
}

/*
 * Lookup in pid entries.
 */
static struct dentry *proc_pident_lookup(struct inode *dir, struct dentry *dentry, const struct pid_entry *ents, size_t nents)
{
	const struct pid_entry *p;
	struct task *task;
	size_t i;

	/* get task */
	task = get_proc_task(dir);
	if (!task)
		return ERR_PTR(-ENOENT);

	for (i = 0; i < nents; i++) {
		p = &ents[i];
		if (p->len == dentry->d_name.len && memcmp(dentry->d_name.name, p->name, p->len) == 0)
			goto found;
	}

	return ERR_PTR(-ENOENT);
found:
	return proc_pident_instantiate(dir, dentry, task, p);
}

/*
 * Read <pid> directory.
 */
static int proc_pid_base_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	return proc_pident_readdir(filp, dirent, filldir, pid_base_entries, ARRAY_SIZE(pid_base_entries));
}

/*
 * Lookup <pid> directory.
 */
static struct dentry *proc_pid_base_lookup(struct inode *dir, struct dentry *dentry)
{
	return proc_pident_lookup(dir, dentry, pid_base_entries, ARRAY_SIZE(pid_base_entries));
}

/*
 * Base <pid> file operations.
 */
static struct file_operations proc_pid_base_dir_fops = {
	.readdir		= proc_pid_base_readdir,
};

/*
 * Base <pid> inode operations.
 */
static struct inode_operations proc_pid_base_dir_iops = {
	.fops			= &proc_pid_base_dir_fops,
	.lookup			= proc_pid_base_lookup,
};

/*
 * Instantiate a base inode.
 */
static struct dentry *proc_base_instantiate(struct inode *dir, struct dentry *dentry, struct task *task, const void *ptr)
{
	const struct pid_entry *p = ptr;
	struct proc_inode_info *ei;
	struct inode *inode;

	/* get an inode */
	inode = get_empty_inode(dir->i_sb);
	if (!inode)
		return ERR_PTR(-ENOMEM);

	/* set inode */
	ei = &inode->u.proc_i;
	inode->i_ino = get_next_ino();
	inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME;
	inode->i_mode = p->mode;
	if (S_ISDIR(inode->i_mode))
		inode->i_nlinks = 2;
	if (S_ISLNK(inode->i_mode))
		inode->i_size = 64;
	if (p->i_op)
		inode->i_op = p->i_op;
	ei->op = p->op;
	ei->pid = task->pid;

	/* set dentry */
	d_add(dentry, inode);
	return NULL;
}

/*
 * Fill a base entry.
 */
static int proc_base_fill_cache(struct file *filp, void *dirent, filldir_t filldir, struct task *task, const struct pid_entry *p)
{
	return proc_fill_cache(filp, dirent, filldir, p->name, p->len, proc_base_instantiate, task, p);
}

/*
 * Instantiate a pid inode.
 */
static struct dentry *proc_pid_instantiate(struct inode *dir, struct dentry *dentry, struct task *task, const void *ptr)
{
	struct inode *inode;

	/* unused variable */
	UNUSED(ptr);

	/* make inode */
	inode = proc_pid_make_inode(dir->i_sb, task);
	if (!inode)
		return ERR_PTR(-ENOENT);

	/* set inode */
	inode->i_mode = S_IFDIR | S_IRUGO | S_IXUGO;
	inode->i_op = &proc_pid_base_dir_iops;
	inode->i_nlinks = 2 + pid_entry_count_dirs(pid_base_entries, ARRAY_SIZE(pid_base_entries));

	/* set dentry */
	dentry->d_op = &pid_dentry_operations;
	d_add(dentry, inode);

	return pid_revalidate(dentry) ? NULL : ERR_PTR(-ENOENT);
}

/*
 * Fill a pid entry.
 */
static int proc_pid_fill_cache(struct file *filp, void *dirent, filldir_t filldir, struct task *task)
{
	char name[PROC_NUMBUF];
	size_t len = sprintf(name, "%d", task->pid);

	return proc_fill_cache(filp, dirent, filldir, name, len, proc_pid_instantiate, task, NULL);
}

/*
 * Read <tid> directory.
 */
static int proc_tid_base_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	return proc_pident_readdir(filp, dirent, filldir, tid_base_entries, ARRAY_SIZE(tid_base_entries));
}

/*
 * Lookup <tid> directory.
 */
static struct dentry *proc_tid_base_lookup(struct inode *dir, struct dentry *dentry)
{
	return proc_pident_lookup(dir, dentry, tid_base_entries, ARRAY_SIZE(tid_base_entries));
}

/*
 * Base <tid> file operations.
 */
static struct file_operations proc_tid_base_dir_fops = {
	.readdir		= proc_tid_base_readdir,
};

/*
 * Base <tid> inode operations.
 */
static struct inode_operations proc_tid_base_dir_iops = {
	.fops			= &proc_tid_base_dir_fops,
	.lookup			= proc_tid_base_lookup,
};

/*
 * Instantiate a tid inode.
 */
static struct dentry *proc_tid_instantiate(struct inode *dir, struct dentry *dentry, struct task *task, const void *ptr)
{
	struct inode *inode;

	/* unused variable */
	UNUSED(ptr);

	/* make inode */
	inode = proc_pid_make_inode(dir->i_sb, task);
	if (!inode)
		return ERR_PTR(-ENOENT);

	/* set inode */
	inode->i_mode = S_IFDIR | S_IRUGO | S_IXUGO;
	inode->i_op = &proc_tid_base_dir_iops;
	inode->i_nlinks = 2 + pid_entry_count_dirs(tid_base_entries, ARRAY_SIZE(tid_base_entries));

	/* set dentry */
	dentry->d_op = &pid_dentry_operations;
	d_add(dentry, inode);

	return pid_revalidate(dentry) ? NULL : ERR_PTR(-ENOENT);
}

/*
 * Fill a tid entry.
 */
static int proc_tid_fill_cache(struct file *filp, void *dirent, filldir_t filldir, struct task *task)
{
	char name[PROC_NUMBUF];
	size_t len = sprintf(name, "%d", task->pid);

	return proc_fill_cache(filp, dirent, filldir, name, len, proc_tid_instantiate, task, NULL);
}

/*
 * Read task directory.
 */
static int proc_task_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	struct dentry *dentry = filp->f_dentry;
	struct inode *inode = dentry->d_inode;
	struct task *task;

	/* get task */
	task = get_proc_task(inode);
	if (!task)
		return -ENOENT;

	/* add "." entry */
	if (filp->f_pos == 0) {
		if (filldir(dirent, ".", 1, filp->f_pos, inode->i_ino))
			return 0;
		filp->f_pos++;
	}

	/* add ".." entry */
	if (filp->f_pos == 1) {
		if (filldir(dirent, "..", 2, filp->f_pos, dentry->d_parent->d_inode->i_ino))
			return 0;
		filp->f_pos++;
	}

	/* add tid entry */
	if (filp->f_pos == 2) {
		if (proc_tid_fill_cache(filp, dirent, filldir, task) < 0)
			return 0;
		filp->f_pos++;
	}

	return 0;
}

/*
 * Lookup task directory.
 */
static struct dentry *proc_task_lookup(struct inode *dir, struct dentry *dentry)
{
	struct task *task;
	pid_t pid;

	/* get task */
	task = get_proc_task(dir);
	if (!task)
		return ERR_PTR(-ENOENT);

	/* parse tid */
	pid = atoi(dentry->d_name.name);
	if (pid != task->pid)
		return ERR_PTR(-ENOENT);

	return proc_tid_instantiate(dir, dentry, task, NULL);
}

/*
 * Task operations.
 */
static struct file_operations proc_task_fops = {
	.readdir		= proc_task_readdir,
};

/*
 * Task inode operations.
 */
static struct inode_operations proc_task_iops = {
	.fops			= &proc_task_fops,
	.lookup			= proc_task_lookup,
};
/*
 * Read pids entries.
 */
int proc_pid_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	size_t nr = filp->f_pos - FIRST_PROCESS_ENTRY, i;
	struct list_head *pos;
	struct pid_entry *p;
	struct task *task;

	/* read base stuff */
	for (; nr < ARRAY_SIZE(proc_base_entries); filp->f_pos++, nr++) {
		p = &proc_base_entries[nr];

		/* fill in fs cache and get entry */
		if (proc_base_fill_cache(filp, dirent, filldir, current_task, p) < 0)
			goto out;
	}

	/* read tasks */
	i = ARRAY_SIZE(proc_base_entries);
	list_for_each(pos, &tasks_list) {
		/* skip init task */
		task = list_entry(pos, struct task, list);
		if (!task || !task->pid)
			continue;

		/* skip first tasks */
		if (i++ < nr)
			continue;

		/* fill in fs cache and get entry */
		if (proc_pid_fill_cache(filp, dirent, filldir, task) < 0)
			break;

		/* update file position */
		filp->f_pos++;
	}

out:
	return 0;
}

/*
 * Lookup a base entry.
 */
static struct dentry *proc_base_lookup(struct inode *dir, struct dentry *dentry)
{
	struct pid_entry *p;
	size_t i;

	/* lookup in base entries */
	for (i = 0; i < ARRAY_SIZE(proc_base_entries); i++) {
		p = &proc_base_entries[i];

		if (p->len == dentry->d_name.len && memcmp(p->name, dentry->d_name.name, p->len) == 0)
			goto found;
	}

	return ERR_PTR(-ENOENT);
found:
	/* instantiate dentry */
	return proc_base_instantiate(dir, dentry, current_task, p);
}

/*
 * Lookup a pid entry.
 */
struct dentry *proc_pid_lookup(struct inode *dir, struct dentry *dentry)
{
	struct dentry *res;
	struct task *task;
	pid_t pid;

	/* lookup in base entries */
	res = proc_base_lookup(dir, dentry);
	if (!IS_ERR(res) || PTR_ERR(res) != -ENOENT)
		return res;

	/* else find task */
	pid = atoi(dentry->d_name.name);
	task = find_task(pid);
	if (!pid || !task)
		return ERR_PTR(-ENOENT);

	/* instantiate dentry */
	return proc_pid_instantiate(dir, dentry, task, NULL);
}