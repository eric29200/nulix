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
static struct proc_dir_entry_t root_dir[] = {
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
static int proc_root_getdents64(struct file_t *filp, void *dirp, size_t count)
{
	struct dirent64_t *dirent = (struct dirent64_t *) dirp;
	struct list_head_t *pos;
	int name_len, ret, n;
	struct task_t *task;
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
		dirent = (struct dirent64_t *) ((void *) dirent + dirent->d_reclen);
	}

	/* add all processes */
	i = NR_ROOT_DIRENTRY;
	list_for_each(pos, &tasks_list) {
		/* skip init task */
		task = list_entry(pos, struct task_t, list);
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
		dirent = (struct dirent64_t *) ((void *) dirent + dirent->d_reclen);

		/* update file position */
		filp->f_pos++;
	}

	return n;
}

/*
 * Root dir lookup.
 */
static int proc_root_lookup(struct inode_t *dir, const char *name, size_t name_len, struct inode_t **res_inode)
{
	struct task_t *task;
	pid_t pid;
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
	for (i = 0; i < NR_ROOT_DIRENTRY; i++) {
		if (proc_match(name, name_len, &root_dir[i])) {
			ino = root_dir[i].ino;
			break;
		}
	}

	/* else try to find matching process */
	if (i >= NR_ROOT_DIRENTRY) {
		pid = atoi(name);
		task = find_task(atoi(name));
		if (!pid || !task) {
			iput(dir);
			return -ENOENT;
		}

		ino = (task->pid << 16) + PROC_PID_INO;
	}

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
 * Root file operations.
 */
struct file_operations_t proc_root_fops = {
	.getdents64		= proc_root_getdents64,
};

/*
 * Root inode operations.
 */
struct inode_operations_t proc_root_iops = {
	.fops			= &proc_root_fops,
	.lookup			= proc_root_lookup,
};

