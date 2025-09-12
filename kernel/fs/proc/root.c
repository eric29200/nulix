#include <fs/proc_fs.h>
#include <proc/sched.h>
#include <stderr.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>

#define NR_ROOT_DIRENTRY		(sizeof(root_dir) / sizeof(root_dir[0]))

/*
 * Root fs directory.
 */
static struct proc_dir_entry root_dir[] = {
	{ PROC_ROOT_INO,	1, 	"." },
	{ PROC_ROOT_INO,	2,	".." },
	{ PROC_UPTIME_INO,	6,	"uptime" },
	{ PROC_FILESYSTEMS_INO,	11,	"filesystems" },
	{ PROC_MOUNTS_INO,	6,	"mounts" },
	{ PROC_KSTAT_INO, 	4,	"stat" },
	{ PROC_SELF_INO,	4,	"self" },
	{ PROC_MEMINFO_INO,	7,	"meminfo" },
	{ PROC_LOADAVG_INO,	7,	"loadavg" },
	{ PROC_NET_INO,		3,	"net" },
};

/*
 * Root read dir.
 */
static int proc_root_getdents64(struct file *filp, void *dirp, size_t count)
{
	struct dirent64 *dirent = (struct dirent64 *) dirp;
	struct list_head *pos;
	int name_len, ret, n;
	struct task *task;
	char pid_s[16];
	size_t i;

	/* read root dir entries */
	for (i = filp->f_pos, n = 0; i < NR_ROOT_DIRENTRY; i++, filp->f_pos++) {
		/* fill in directory entry */ 
		ret = filldir(dirent, root_dir[i].name, root_dir[i].name_len, root_dir[i].ino, count);
		if (ret)
			return n;

		/* go to next dir entry */
		count -= dirent->d_reclen;
		n += dirent->d_reclen;
		dirent = (struct dirent64 *) ((void *) dirent + dirent->d_reclen);
	}

	/* add all processes */
	i = NR_ROOT_DIRENTRY;
	list_for_each(pos, &tasks_list) {
		/* skip init task */
		task = list_entry(pos, struct task, list);
		if (!task || !task->pid)
			continue;

		/* skip processes before offset */
		if (filp->f_pos > i++)
			continue;

		/* fill in directory entry */ 
		name_len = sprintf(pid_s, "%d", task->pid);
		ret = filldir(dirent, pid_s, name_len, (task->pid << 16) + PROC_PID_INO, count);
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
 * Root dir lookup.
 */
static struct dentry *proc_root_lookup(struct inode *dir, struct dentry *dentry)
{
	struct inode *inode = NULL;
	struct task *task;
	pid_t pid;
	ino_t ino;
	size_t i;

	/* find matching entry */
	for (i = 0; i < NR_ROOT_DIRENTRY; i++) {
		if (proc_match(dentry->d_name.name, dentry->d_name.len, &root_dir[i])) {
			ino = root_dir[i].ino;
			break;
		}
	}

	/* else try to find matching process */
	if (i >= NR_ROOT_DIRENTRY) {
		pid = atoi(dentry->d_name.name);
		task = find_task(pid);
		if (!pid || !task)
			return ERR_PTR(-ENOENT);

		ino = (task->pid << 16) + PROC_PID_INO;
	}

	/* get inode */
	inode = iget(dir->i_sb, ino);
	if (!inode)
		return ERR_PTR(-EACCES);

	d_add(dentry, inode);
	return NULL;
}

/*
 * Root file operations.
 */
struct file_operations proc_root_fops = {
	.getdents64		= proc_root_getdents64,
};

/*
 * Root inode operations.
 */
struct inode_operations proc_root_iops = {
	.fops			= &proc_root_fops,
	.lookup			= proc_root_lookup,
};

