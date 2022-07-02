#include <fs/fs.h>
#include <fs/minix_fs.h>
#include <fs/proc_fs.h>
#include <proc/sched.h>
#include <mm/mm.h>
#include <stdio.h>
#include <stderr.h>
#include <string.h>
#include <fcntl.h>
#include <dev.h>

/* file systems list */
static LIST_HEAD(fs_list);

/*
 * Register a file system.
 */
int register_filesystem(struct file_system_t *fs)
{
	if (!fs)
		return -EINVAL;

	/* add file system */
	list_add(&fs->list, &fs_list);

	return 0;
}

/*
 * Get a file system.
 */
struct file_system_t *get_filesystem(const char *name)
{
	struct file_system_t *fs;
	struct list_head_t *pos;

	if (!name)
		return NULL;

	/* search file system */
	list_for_each(pos, &fs_list) {
		fs = list_entry(pos, struct file_system_t, list);
		if (strcmp(fs->name, name) == 0)
			return fs;
	}

	return NULL;
}

/*
 * Get mount point inode.
 */
static int get_mount_point(const char *mount_point, struct inode_t **res_inode)
{
	/* get mount point */
	*res_inode = namei(AT_FDCWD, NULL, mount_point, 1);
	if (!*res_inode)
		return -EINVAL;

	/* mount point busy */
	if ((*res_inode)->i_ref != 1 || (*res_inode)->i_mount) {
		iput(*res_inode);
		return -EBUSY;
	}

	/* mount point must be a directory */
	if (!S_ISDIR((*res_inode)->i_mode)) {
		iput(*res_inode);
		return -EPERM;
	}

	return 0;
}

/*
 * Mount a file system.
 */
int do_mount(struct file_system_t *fs, dev_t dev, const char *mount_point, void *data, int flags)
{
	struct inode_t *mount_point_dir = NULL;
	struct super_block_t *sb;
	int err;

	/* allocate a super block */
	sb = (struct super_block_t *) kmalloc(sizeof(struct super_block_t));
	if (!sb)
		return -ENOMEM;

	/* get mount point */
	err = get_mount_point(mount_point, &mount_point_dir);
	if (err)
		goto err;

	/* read super block */
	sb->s_dev = dev;
	err = fs->read_super(sb, data, flags);
	if (err)
		goto err;

	/* set mount point */
	mount_point_dir->i_mount = sb->s_root_inode;

	return 0;
err:
	if (mount_point_dir)
		iput(mount_point_dir);

	kfree(sb);
	return err;
}

/*
 * Mount root file system.
 */
int do_mount_root(dev_t dev)
{
	struct file_system_t *fs;
	struct super_block_t *sb;
	struct list_head_t *pos;
	int err;

	/* allocate a super block */
	sb = (struct super_block_t *) kmalloc(sizeof(struct super_block_t));
	if (!sb)
		return -ENOMEM;

	/* set device */
	sb->s_dev = dev;

	/* try all file systems */
	list_for_each(pos, &fs_list) {
		/* test only dev file systems */
		fs = list_entry(pos, struct file_system_t, list);
		if (!fs->requires_dev)
			continue;

		/* read super block */
		err = fs->read_super(sb, NULL, 0);
		if (err == 0)
			goto found;
	}

	kfree(sb);
	return -EINVAL;
found:
	/* set mount point */
	sb->s_root_inode->i_ref = 3;
	current_task->cwd = sb->s_root_inode;
	current_task->root = sb->s_root_inode;
	strcpy(current_task->cwd_path, "/");

	return 0;
}
