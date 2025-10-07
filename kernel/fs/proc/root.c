#include <fs/proc_fs.h>
#include <proc/sched.h>
#include <stderr.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>

#define FIRST_PROCESS_ENTRY		256
#define PROC_MAXPIDS			20
#define PROC_NUMBUF			10

/*
 * Root entries.
 */
struct proc_dir_entry proc_root = {
	PROC_ROOT_INO, 5, "/proc", S_IFDIR | S_IRUGO | S_IXUGO, 2, 0, 0, 0,
	&proc_root_iops, NULL, &proc_root, NULL, NULL
};
static struct proc_dir_entry proc_root_self = {
	PROC_SELF_INO, 4, "self", S_IFLNK | S_IRUGO | S_IWUGO | S_IXUGO, 1, 0, 0, 64,
	&proc_self_iops, NULL, NULL, NULL, NULL
};
struct proc_dir_entry *proc_net;

/*
 * Init root entries.
 */
void proc_root_init()
{
	proc_base_init();
	proc_misc_init();
	proc_register(&proc_root, &proc_root_self);
	proc_net = proc_mkdir("net", 0);
}

/*
 * Get pids list.
 */
static int get_pid_list(int index, ino_t *pids)
{
	struct list_head *pos;
	struct task *task;
	int nr_pids = 0;

	index -= FIRST_PROCESS_ENTRY;

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
 * Root read dir.
 */
static int proc_root_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	size_t nr = filp->f_pos, nr_pids, i, j;
	ino_t pid_array[PROC_MAXPIDS];
	char buf[PROC_NUMBUF];
	pid_t pid;
	ino_t ino;
	int ret;

	/* read global entries */
	if (nr < FIRST_PROCESS_ENTRY) {
		ret = proc_readdir(filp, dirent, filldir);
		if (ret <= 0)
			return ret;
		filp->f_pos = nr = FIRST_PROCESS_ENTRY;
	}

	/* get pids list */
	nr_pids = get_pid_list(nr, pid_array);

	/* add pids entries */
	for (i = 0; i < nr_pids; i++) {
		pid = pid_array[i];
		ino = (pid << 16) + PROC_PID_INO;
		j = PROC_NUMBUF;

		do {
			j--;
			buf[j] = '0' + (pid % 10);
			pid /= 10;
		} while (pid);

		if (filldir(dirent, buf+j, PROC_NUMBUF-j, filp->f_pos, ino) < 0)
			break;

		filp->f_pos++;
	}

	return 0;
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

	/* find matching entry */
	if (!proc_lookup(dir, dentry))
		return NULL;

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
 * Root file operations.
 */
static struct file_operations proc_root_fops = {
	.readdir		= proc_root_readdir,
};

/*
 * Root inode operations.
 */
struct inode_operations proc_root_iops = {
	.fops			= &proc_root_fops,
	.lookup			= proc_root_lookup,
};