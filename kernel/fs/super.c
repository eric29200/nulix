#include <fs/fs.h>
#include <fs/mount.h>
#include <fs/minix_fs.h>
#include <fs/proc_fs.h>
#include <proc/sched.h>
#include <drivers/block/blk_dev.h>
#include <mm/mm.h>
#include <stdio.h>
#include <stderr.h>
#include <string.h>
#include <fcntl.h>
#include <dev.h>

/* file systems and mounted fs lists */
static LIST_HEAD(fs_list);
static LIST_HEAD(vfs_mounts_list);

/*
 * Register a file system.
 */
int register_filesystem(struct file_system *fs)
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
struct file_system *get_filesystem(const char *name)
{
	struct file_system *fs;
	struct list_head *pos;

	if (!name)
		return NULL;

	/* search file system */
	list_for_each(pos, &fs_list) {
		fs = list_entry(pos, struct file_system, list);
		if (strcmp(fs->name, name) == 0)
			return fs;
	}

	return NULL;
}

/*
 * Get file systems list.
 */
int get_filesystem_list(char *buf, int count)
{
	struct file_system *fs;
	struct list_head *pos;
	int len = 0;

	list_for_each(pos, &fs_list) {
		/* check overflow */
		if (len >= count - 80)
			break;

		fs = list_entry(pos, struct file_system, list);
		len += sprintf(buf + len, "%s\t%s\n", fs->requires_dev ? "" : "nodev", fs->name);
	}

	return len;
}

/*
 * Get mounted file systems list.
 */
int get_vfs_mount_list(char *buf, int count)
{
	struct vfs_mount *vfs_mount;
	struct list_head *pos;
	int len = 0;

	list_for_each(pos, &vfs_mounts_list) {
		/* check overflow */
		if (len >= count - 160)
			break;

		vfs_mount = list_entry(pos, struct vfs_mount, mnt_list);
		len += sprintf(buf + len, "%s %s %s %s 0 0\n",
			       vfs_mount->mnt_devname,
			       vfs_mount->mnt_dirname,
			       vfs_mount->mnt_sb->s_type->name,
			       vfs_mount->mnt_flags & MS_RDONLY ? "ro" : "rw");
	}

	return len;
}

/*
 * Add a mounted file system.
 */
static int add_vfs_mount(dev_t dev, const char *dev_name, const char *dir_name, int flags, struct super_block *sb)
{
	struct vfs_mount *vfs_mount;

	/* allocate a new mount fs */
	vfs_mount = (struct vfs_mount *) kmalloc(sizeof(struct vfs_mount));
	if (!vfs_mount)
		return -ENOMEM;

	/* init mount fs */
	vfs_mount->mnt_dev = dev;
	vfs_mount->mnt_devname = NULL;
	vfs_mount->mnt_dirname = NULL;
	vfs_mount->mnt_flags = flags;
	vfs_mount->mnt_sb = sb;

	/* set device name */
	if (dev_name) {
		vfs_mount->mnt_devname = strdup(dev_name);
		if (!vfs_mount->mnt_devname)
			goto err;
	} else {
		vfs_mount->mnt_devname = NULL;
	}

	/* set dir name */
	if (dir_name) {
		vfs_mount->mnt_dirname = strdup(dir_name);
		if (!vfs_mount->mnt_dirname)
			goto err;
	} else {
		vfs_mount->mnt_dirname = NULL;
	}

	/* add mounted file system */
	list_add(&vfs_mount->mnt_list, &vfs_mounts_list);

	return 0;
err:
	if (vfs_mount->mnt_devname)
		kfree(vfs_mount->mnt_devname);

	if (vfs_mount->mnt_dirname)
		kfree(vfs_mount->mnt_dirname);

	kfree(vfs_mount);
	return -ENOMEM;
}

/*
 * Find a mounted file system.
 */
static struct vfs_mount *lookup_vfs_mount(struct super_block *sb)
{
	struct vfs_mount *vfs_mount;
	struct list_head *pos;

	/* find mounted file system */
	list_for_each(pos, &vfs_mounts_list) {
		vfs_mount = list_entry(pos, struct vfs_mount, mnt_list);
		if (vfs_mount->mnt_sb == sb)
			return vfs_mount;
	}

	return NULL;
}

/*
 * Remove a mounted file system.
 */
static void del_vfs_mount(struct super_block *sb)
{
	struct vfs_mount *vfs_mount;

	/* find mounted file system */
	vfs_mount = lookup_vfs_mount(sb);
	if (!vfs_mount)
		return;

	/* remove file system from list */
	list_del(&vfs_mount->mnt_list);

	/* free file system */
	if (vfs_mount->mnt_devname)
		kfree(vfs_mount->mnt_devname);
	if (vfs_mount->mnt_dirname)
		kfree(vfs_mount->mnt_dirname);
	kfree(vfs_mount);
}

/*
 * Check if a file system can be remounted read only.
 */
static int fs_may_remount_ro(struct super_block *sb)
{
	struct inode *inode;
	struct file *filp;
	size_t i;

	for (i = 0; i < NR_FILE; i++) {
		filp = &filp_table[i];

		if (!filp->f_count)
			continue;
		if (!filp->f_dentry || !filp->f_dentry->d_inode)
			continue;

		inode = filp->f_dentry->d_inode;
		if (!inode || inode->i_sb != sb)
			continue;
		if (S_ISREG(inode->i_mode) && (filp->f_mode & MAY_WRITE))
			return 0;
	}

	return 1;
}

/*
 * Remount a file system.
 */
static int do_remount_sb(struct super_block *sb, uint32_t flags)
{
	struct vfs_mount *vfs_mount;

	/* read only device */
	if (!(flags & MS_RDONLY) && sb->s_dev && is_read_only(sb->s_dev))
		return -EACCES;

	/* check if we can remount read only */
	if ((flags & MS_RDONLY) && !(sb->s_flags & MS_RDONLY))
		if (!fs_may_remount_ro(sb))
			return -EBUSY;

	/* update flags */
	sb->s_flags = flags;

	/* update mounted file system */
	vfs_mount = lookup_vfs_mount(sb);
	if (vfs_mount)
		vfs_mount->mnt_flags = sb->s_flags;

	return 0;
}

/*
 * Remount a file system.
 */
static int do_remount(const char *dir_name, uint32_t flags)
{
	struct inode *dir_inode;
	struct dentry *dentry;
	int ret;

	/* resolve path */
	dentry = namei(AT_FDCWD, dir_name, 1);
	if (IS_ERR(dentry))
		return PTR_ERR(dentry);

	/* get inode */
	dir_inode = dentry->d_inode;

	/* check mount point */
	ret = -EINVAL;
	if (dir_inode != dir_inode->i_sb->s_root->d_inode)
		goto out;

	/* remount */
	ret = do_remount_sb(dir_inode->i_sb, flags);
out:
	dput(dentry);
	return ret;
}

/*
 * Set mount.
 */
static void d_mount(struct dentry *covers, struct dentry *dentry)
{
	if (covers->d_mounts != covers) {
		printf("d_mount: mount - already mounted\n");
		return;
	}

	covers->d_mounts = dentry;
	dentry->d_covers = covers;
}

/*
 * Mount a file system.
 */
static int do_mount(struct file_system *fs, dev_t dev, const char *dev_name, const char *mount_point, void *data, uint32_t flags)
{
	struct dentry *mount_point_dentry = NULL;
	struct super_block *sb;
	int ret;

	/* allocate a super block */
	sb = (struct super_block *) kmalloc(sizeof(struct super_block));
	if (!sb)
		return -ENOMEM;

	/* resolove mount point */
	mount_point_dentry = lookup_dentry(AT_FDCWD, NULL, mount_point, 1);
	ret = PTR_ERR(mount_point_dentry);
	if (IS_ERR(mount_point_dentry))
		goto err;

	/* no such entry */
	ret = -ENOENT;
	if (!mount_point_dentry->d_inode)
		goto err;

	/* mount point busy */
	ret = -EBUSY;
	if (mount_point_dentry->d_covers != mount_point_dentry)
		goto err;

	/* not a directory */
	ret = -ENOTDIR;
	if (!S_ISDIR(mount_point_dentry->d_inode->i_mode))
		goto err;

	/* read super block */
	sb->s_type = fs;
	sb->s_dev = dev;
	ret = fs->read_super(sb, data, 0);
	if (ret)
		goto err;

	/* check mount */
	ret = -EBUSY;
	if (sb->s_root->d_covers != sb->s_root)
		goto err;

	/* add mounted file system */
	ret = add_vfs_mount(dev, dev_name, mount_point, flags, sb);
	if (ret)
		goto err;

	/* set mount */
	d_mount(mount_point_dentry, sb->s_root);

	return 0;
err:
	dput(mount_point_dentry);
	kfree(sb);
	return ret;
}

/*
 * Mount system call.
 */
int sys_mount(char *dev_name, char *dir_name, char *type, unsigned long flags, void *data)
{
	struct file_system *fs;
	struct dentry *dentry;
	struct inode *inode;
	dev_t dev = 0;

	/* only root can mount */
	if (!suser())
		return -EPERM;

	/* remount ? */
	if (flags & MS_REMOUNT)
		return do_remount(dir_name, flags);

	/* find file system type */
	fs = get_filesystem(type);
	if (!fs)
		return -ENODEV;

	/* if filesystem requires dev, find it */
	if (fs->requires_dev) {
		/* find device's inode */
		dentry = namei(AT_FDCWD, dev_name, 1);
		if (IS_ERR(dentry))
			return PTR_ERR(dentry);

		/* get inode */
		inode = dentry->d_inode;

		/* must be a block device */
		if (!S_ISBLK(inode->i_mode)) {
			dput(dentry);
			return -ENOTBLK;
		}

		/* get device */
		dev = inode->i_rdev;
		dput(dentry);
	}

	return do_mount(fs, dev, dev_name, dir_name, data, flags);
}

/*
 * Mount root file system.
 */
int do_mount_root(dev_t dev, const char *dev_name)
{
	struct file_system *fs;
	struct super_block *sb;
	struct list_head *pos;
	int err;

	/* allocate a super block */
	sb = (struct super_block *) kmalloc(sizeof(struct super_block));
	if (!sb)
		return -ENOMEM;

	/* set device */
	sb->s_dev = dev;
	sb->s_flags = MS_RDONLY;

	/* try all file systems */
	list_for_each(pos, &fs_list) {
		/* test only dev file systems */
		fs = list_entry(pos, struct file_system, list);
		if (!fs->requires_dev)
			continue;

		/* read super block */
		sb->s_type = fs;
		err = fs->read_super(sb, NULL, 1);
		if (err == 0)
			goto found;
	}

	kfree(sb);
	return -EINVAL;
found:
	/* set current task */
	current_task->fs->pwd = dget(sb->s_root);
	current_task->fs->root = dget(sb->s_root);

	/* add mounted file system */
	err = add_vfs_mount(dev, dev_name, "/", 0, sb);
	if (err) {
		kfree(sb);
		return err;
	}

	return 0;
}

/*
 * Unset mount.
 */
static void d_umount(struct dentry *dentry)
{
	struct dentry *covers = dentry->d_covers;

	if (covers == dentry) {
		printf("d_umount: unmount - covers == dentry ?\n");
		return;
	}

	covers->d_mounts = covers;
	dentry->d_covers = dentry;
	dput(covers);
	dput(dentry);
}

/*
 * Unmount a file system.
 */
static int do_umount(const char *target, int flags)
{
	struct super_block *sb;
	struct dentry *dentry;
	struct inode *inode;

	/* unused flags */
	UNUSED(flags);

	/* resolve path */
	dentry = namei(AT_FDCWD, target, 1);
	if (IS_ERR(dentry))
		return PTR_ERR(dentry);

	/* target inode must be a root inode */
	inode = dentry->d_inode;
	sb = inode->i_sb;
	if (!sb || inode != sb->s_root->d_inode) {
		dput(dentry);
		return -EINVAL;
	}

	/* release inode */
	dput(dentry);

	/* check if file system is busy */
	if (!fs_may_umount(sb))
		return -EBUSY;

	/* sync buffers */
	sync_dev(sb->s_dev);

	/* unset mount */
	if (sb->s_root) {
		d_umount(sb->s_root);
		d_delete(sb->s_root);
	}

	/* remove mounted file system */
	del_vfs_mount(sb);

	/* release super block */
	if (sb->s_op && sb->s_op->put_super)
		sb->s_op->put_super(sb);

	return 0;
}

/*
 * Umount system call.
 */
int sys_umount(const char *target)
{
	return sys_umount2(target, 0);
}

/*
 * Umount2 system call.
 */
int sys_umount2(const char *target, int flags)
{
	/* only root can umount */
	if (!suser())
		return -EPERM;

	return do_umount(target, flags);
}
