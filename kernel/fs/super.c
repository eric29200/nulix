#include <fs/fs.h>
#include <fs/minix_fs.h>
#include <fs/proc_fs.h>
#include <proc/sched.h>
#include <mm/mm.h>
#include <stdio.h>
#include <stderr.h>
#include <string.h>
#include <fcntl.h>

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
int do_mount(uint16_t magic, dev_t dev, const char *mount_point)
{
	struct inode_t *mount_point_dir = NULL;
	struct super_block_t *sb;
	int err, is_root;

	/* allocate a super block */
	sb = (struct super_block_t *) kmalloc(sizeof(struct super_block_t));
	if (!sb)
		return -ENOMEM;

	/* check if mount root */
	is_root = mount_point != NULL && strlen(mount_point) == 1 && *mount_point == '/';

	/* get mount point */
	if (!is_root) {
		err = get_mount_point(mount_point, &mount_point_dir);
		if (err)
			goto err;
	}

	/* read super block */
	switch (magic) {
		case MINIX_SUPER_MAGIC:
			err = minix_read_super(sb, dev);
			break;
		case PROC_SUPER_MAGIC:
			err = proc_read_super(sb);
			break;
		default:
			err = -EINVAL;
			break;
	}

	/* no way to read super block */
	if (err)
		goto err;

	/* set mount point */
	if (is_root) {
		sb->s_root_inode->i_ref = 3;
		current_task->cwd = sb->s_root_inode;
		current_task->root = sb->s_root_inode;
		strcpy(current_task->cwd_path, "/");
	} else {
		mount_point_dir->i_mount = sb->s_root_inode;
	}

	return 0;
err:
	if (mount_point_dir)
		iput(mount_point_dir);

	kfree(sb);
	return err;
}
