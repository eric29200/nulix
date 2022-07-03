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
};

/*
 * Root read dir.
 */
static int proc_root_getdents64(struct file_t *filp, void *dirp, size_t count)
{
	struct dirent64_t *dirent;
	struct list_head_t *pos;
	struct task_t *task;
	int name_len, n;
	char pid_s[16];
	size_t i;

	/* read root dir entries */
	for (i = filp->f_pos, n = 0, dirent = (struct dirent64_t *) dirp; i < NR_ROOT_DIRENTRY; i++, filp->f_pos++) {
		/* check buffer size */
		name_len = root_dir[i].name_len;
		if (count < sizeof(struct dirent64_t) + name_len + 1)
			return n;

		/* set dir entry */
		dirent->d_inode = root_dir[i].ino;
		dirent->d_type = 0;
		memcpy(dirent->d_name, root_dir[i].name, name_len);
		dirent->d_name[name_len] = 0;

		/* set dir entry size */
		dirent->d_reclen = sizeof(struct dirent64_t) + name_len + 1;

		/* go to next dir entry */
		count -= dirent->d_reclen;
		n += dirent->d_reclen;
		dirent = (struct dirent64_t *) ((void *) dirent + dirent->d_reclen);
	}

	/* add all processes */
	i = 2;
	list_for_each(pos, &tasks_list) {
		/* skip init task */
		task = list_entry(pos, struct task_t, list);
		if (!task || !task->pid)
			continue;

		/* skip processes before offset */
		if (filp->f_pos > i++)
			continue;

		/* check buffer size */
		name_len = sprintf(pid_s, "%d", task->pid);
		if (count < sizeof(struct dirent64_t) + name_len + 1)
			return n;

		/* set dir entry */
		dirent->d_inode = task->pid + PROC_BASE_INO;
		dirent->d_type = 0;
		memcpy(dirent->d_name, pid_s, name_len);
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
		ino = PROC_BASE_INO + task->pid;
	}

	/* get inode */
	*res_inode = iget(dir->i_sb, ino);
	if (!*res_inode) {
		iput(dir);
		return -EACCES;
	}

	/* uptime file */
	if (ino == PROC_UPTIME_INO) {
		(*res_inode)->i_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH;
		(*res_inode)->i_op = &proc_uptime_iops;
		goto out;
	}

	/* file systems file */
	if (ino == PROC_FILESYSTEMS_INO) {
		(*res_inode)->i_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH;
		(*res_inode)->i_op = &proc_filesystems_iops;
		goto out;
	}

	/* mounts file */
	if (ino == PROC_MOUNTS_INO) {
		(*res_inode)->i_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH;
		(*res_inode)->i_op = &proc_mounts_iops;
		goto out;
	}

	/* base process file */
	if (ino >= PROC_BASE_INO) {
		(*res_inode)->i_mode = S_IFDIR | S_IRUSR | S_IRGRP | S_IROTH | S_IXUSR | S_IXGRP | S_IXOTH;
		(*res_inode)->i_nlinks = 2;
		(*res_inode)->i_op = &proc_base_iops;
		goto out;
	}

out:
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

