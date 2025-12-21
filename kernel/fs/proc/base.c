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
#define PROC_PID_ROOT			6
#define PROC_PID_CWD			7
#define PROC_PID_EXE			8
#define PROC_PID_CMDLINE		9
#define PROC_PID_ENVIRON		10
#define PROC_PID_MAPS			11
#define PROC_PID_FD			12

#define MAPS_LINE_SHIFT			12
#define MAPS_LINE_LENGTH		(1 << MAPS_LINE_SHIFT)

#define fake_ino(pid, ino)		(((pid) << 16) | (ino))

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

#define NOD(TYPE, NAME, MODE, IOP, OP) {		\
	.type = TYPE,					\
	.name = (NAME),					\
	.len  = sizeof(NAME) - 1,			\
	.mode = MODE,					\
	.i_op = IOP,					\
	.op   = OP,					\
}

#define REG(type, NAME, MODE, IOP)		NOD(type, NAME, (S_IFREG | (MODE)), &IOP, {})
#define INF(type, NAME, MODE, read)		NOD(type, NAME, (S_IFREG | (MODE)), &proc_base_file_iops, { .proc_read = read })
#define LNK(type, NAME, get_link)		NOD(type, NAME, (S_IFLNK | S_IRWXUGO), &proc_base_link_iops, { .proc_get_link = get_link })
#define DIR(type, NAME, MODE, IOP)		NOD(type, NAME, (S_IFDIR | (MODE)), &IOP, {})

static struct inode_operations proc_base_file_iops;
static struct inode_operations proc_base_maps_iops;
static struct inode_operations proc_base_link_iops;
static struct inode_operations proc_fd_iops;
static int proc_root_link(struct task *task, struct dentry **dentry);
static int proc_cwd_link(struct task *task, struct dentry **dentry);
static int proc_exe_link(struct task *task, struct dentry **dentry);

/*
 * <pid> entries.
 */
static struct pid_entry pid_base_entries[] = {
	INF(PROC_PID_STAT,	"stat",		S_IRUGO,		proc_stat_read		),
	INF(PROC_PID_STATUS,	"status",	S_IRUGO,		proc_status_read	),
	INF(PROC_PID_STATM,	"statm",	S_IRUGO,		proc_statm_read		),
	INF(PROC_PID_CMDLINE,	"cmdline",	S_IRUGO,		proc_cmdline_read	),
	INF(PROC_PID_ENVIRON,	"environ",	S_IRUGO,		proc_environ_read	),
	REG(PROC_PID_MAPS,	"maps",		S_IRUGO,		proc_base_maps_iops	),
	LNK(PROC_PID_ROOT,	"root",		proc_root_link					),
	LNK(PROC_PID_CWD,	"cwd",		proc_cwd_link					),
	LNK(PROC_PID_EXE,	"exe",		proc_exe_link					),
	DIR(PROC_PID_FD,	"fd",		S_IRUSR | S_IXUSR,	proc_fd_iops		),
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
	struct task *task = inode->u.proc_i.task;
	struct dentry *res;
	int ret;

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
	struct task *task = inode->u.proc_i.task;
	struct dentry *res;
	int ret;

	/* release base */
	dput(base);

	/* check permissions */
	ret = permission(inode, MAY_EXEC);
	if (ret)
		return ERR_PTR(ret);

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
 * Read /<pid>/maps
 */
static ssize_t proc_base_maps_read(struct file *filp, char *buf, size_t count, off_t *ppos)
{
	struct inode *inode = filp->f_dentry->d_inode;
	char *page, *line, perms[5], *destptr = buf;
	struct task *task = inode->u.proc_i.task;
	ssize_t column, i = 0, len;
	size_t j, maxlen = 60;
	struct list_head *pos;
	struct vm_area *vma;
	off_t lineno;
	dev_t dev;
	ino_t ino;
	int err;

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
 * Read <pid> directory.
 */
static int proc_pid_base_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	struct dentry *dentry = filp->f_dentry;
	struct inode *inode = dentry->d_inode;
	struct pid_entry *entry;
	size_t i = filp->f_pos;
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
		if (filldir(dirent, "..", 2, i, dentry->d_parent->d_inode->i_ino) < 0)
			return 0;
		i++;
		filp->f_pos++;
	}

	/* add entries */
	i -= 2;
	if (i >= sizeof(pid_base_entries) / sizeof(pid_base_entries[0]))
		return 1;
	for (entry = &pid_base_entries[i]; entry->name; entry++) {
		if (filldir(dirent, entry->name, entry->len, filp->f_pos, fake_ino(pid, entry->type)) < 0)
			return 0;
		filp->f_pos++;
	}

	return 0;
}

/*
 * Lookup <pid> directory.
 */
static struct dentry *proc_pid_base_lookup(struct inode *dir, struct dentry *dentry)
{
	struct task *task = dir->u.proc_i.task;
	struct pid_entry *entry;
	struct inode *inode;

	/* find entry */
	for (entry = pid_base_entries; entry->name; entry++) {
		if (entry->len != dentry->d_name.len)
			continue;
		if (!memcmp(dentry->d_name.name, entry->name, entry->len))
			break;
	}

	/* no matching entry */
	if (!entry->name)
		return ERR_PTR(-ENOENT);

	/* make an inode */
	inode = proc_pid_make_inode(dir->i_sb, task, entry->type);
	if (!inode)
		return ERR_PTR(-EINVAL);

	/* set inode */
	inode->i_mode = entry->mode;
	inode->i_op = entry->i_op;
	inode->u.proc_i.op = entry->op;
	if (S_ISDIR(inode->i_mode))
		inode->i_nlinks = 2;

	dentry->d_op = &pid_dentry_operations;
	d_add(dentry, inode);
	return NULL;
}

/*
 * Base <pid> file operations.
 */
static struct file_operations proc_pid_base_fops = {
	.readdir		= proc_pid_base_readdir,
};

/*
 * Base <pid> inode operations.
 */
static struct inode_operations proc_pid_base_iops = {
	.fops			= &proc_pid_base_fops,
	.lookup			= proc_pid_base_lookup,
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
	inode->i_op = &proc_pid_base_iops;
	inode->i_nlinks = 3;

	dentry->d_op = &pid_base_dentry_operations;
	d_add(dentry, inode);
	return NULL;
}
