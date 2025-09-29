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
static struct file_system_type *file_systems = NULL;
static LIST_HEAD(vfs_mounts_list);
static LIST_HEAD(super_blocks);
static size_t nr_super_blocks;

/* unnamed devices */
static uint32_t unnamed_dev_in_use[256 / (8 * sizeof(uint32_t))] = { 0, };

/*
 * Get an unnamed device.
 */
static dev_t get_unnamed_dev()
{
	int i;

	for (i = 1; i < 256; i++) {
		if (!test_bit(unnamed_dev_in_use, i)) {
			set_bit(unnamed_dev_in_use, i);
		 	return mkdev(DEV_UNNAMED_MAJOR, i);
		}
	}

	return 0;
}

/*
 * Release an unnamed device.
 */
void put_unnamed_dev(dev_t dev)
{
	/* check device */
	if (!dev || major(dev) != DEV_UNNAMED_MAJOR)
		return;

	/* clear device */
	clear_bit(unnamed_dev_in_use, minor(dev));
}

/*
 * Get a filesystem.
 */
static struct file_system_type *get_fs_type(const char *name)
{
	struct file_system_type *fs = file_systems;

	if (!name)
		return fs;

	for (fs = file_systems; fs; fs = fs->next)
		if (strcmp(fs->name, name) == 0)
			return fs;

	return NULL;
}

/*
 * Get filesystems list.
 */
int get_filesystem_list(char *buf, int count)
{
	struct file_system_type *fs;
	int len = 0;

	for (fs = file_systems; fs; fs = fs->next) {
		/* check overflow */
		if (len >= count - 80)
			break;

		len += sprintf(buf + len, "%s\t%s\n", (fs->flags & FS_REQUIRES_DEV) ? "" : "nodev", fs->name);
	}

	return len;
}

/*
 * Register a file system.
 */
int register_filesystem(struct file_system_type *fs)
{
	struct file_system_type **tmp;

	/* check filesystem */
	if (!fs)
		return -EINVAL;

	/* check filesystem */
	if (fs->next)
		return -EBUSY;

	/* find filesystem */
	for (tmp = &file_systems; *tmp; tmp = &(*tmp)->next)
                if (strcmp((*tmp)->name, fs->name) == 0)
                        return -EBUSY;

	/* register filesystem */
	*tmp = fs;

	return 0;
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
 * Get an empty super block.
 */
static struct super_block *get_empty_super()
{
	struct super_block *sb;
	struct list_head *pos;

	/* try to get an unused super block */
	list_for_each(pos, &super_blocks) {
		sb = list_entry(pos, struct super_block, s_list);
		if (!sb->s_dev)
			return sb;
	}

	/* maximum number of super blocks reached */
	if (nr_super_blocks >= NR_SUPER)
		return NULL;

	/* allocate a new super block */
	sb = (struct super_block *) kmalloc(sizeof(struct super_block));
	if (!sb)
		return NULL;

	/* init super block */
	nr_super_blocks++;
	memset(sb, 0, sizeof(struct super_block));
	list_add(&sb->s_list, &super_blocks);

	return sb;
}

/*
 * Get a super block.
 */
struct super_block *get_super(dev_t dev)
{
	struct super_block *sb;
	struct list_head *pos;

	/* check device */
	if (!dev)
		return NULL;

	/* find super block */
	list_for_each(pos, &super_blocks) {
		sb = list_entry(pos, struct super_block, s_list);
		if (sb->s_dev == dev)
			return sb;
	}

	return NULL;
}

/*
 * Read a super block.
 */
static struct super_block *read_super(dev_t dev, const char *name, int flags, void *data, int silent)
{
	struct file_system_type *fs;
	struct super_block *sb;
	int ret;

	/* check device */
	if (!dev)
		return NULL;

	/* get super block */
	sb = get_super(dev);
	if (sb)
		return sb;

	/* get filesystem */
	fs = get_fs_type(name);
	if (!fs)
		return NULL;

	/* get an empty super block */
	sb = get_empty_super();
	if (!sb)
		return NULL;

	/* set super block */
	sb->s_dev = dev;
	sb->s_flags = flags;
	sb->s_type = fs;

	/* read super block */
	ret = fs->read_super(sb, data, silent);
	if (ret) {
		sb->s_dev = 0;
		return NULL;
	}

	return sb;
}

/*
 * Mount a file system.
 */
static int do_mount(dev_t dev, const char *dev_name, const char *dir_name, const char *type, int flags, void *data)
{
	struct dentry *dir_dentry = NULL;
	struct super_block *sb;
	int ret;

	/* resolove mount point */
	dir_dentry = lookup_dentry(AT_FDCWD, NULL, dir_name, 1);
	ret = PTR_ERR(dir_dentry);
	if (IS_ERR(dir_dentry))
		goto err;

	/* no such entry */
	ret = -ENOENT;
	if (!dir_dentry->d_inode)
		goto err;

	/* mount point busy */
	ret = -EBUSY;
	if (dir_dentry->d_covers != dir_dentry)
		goto err;

	/* not a directory */
	ret = -ENOTDIR;
	if (!S_ISDIR(dir_dentry->d_inode->i_mode))
		goto err;

	/* get an empty super block */
	ret = -EINVAL;
	sb = read_super(dev, type, flags, data, 0);
	if (!sb)
		goto err;

	/* check mount */
	ret = -EBUSY;
	if (sb->s_root->d_count != 1 || sb->s_root->d_covers != sb->s_root)
		goto err;

	/* add mounted file system */
	ret = add_vfs_mount(dev, dev_name, dir_name, flags, sb);
	if (ret)
		goto err;

	/* set mount */
	d_mount(dir_dentry, sb->s_root);

	return 0;
err:
	dput(dir_dentry);
	return ret;
}

/*
 * Mount system call.
 */
int sys_mount(char *dev_name, char *dir_name, char *type, unsigned long flags, void *data)
{
	struct file_system_type *fs;
	struct dentry *dentry;
	struct inode *inode;
	dev_t dev;

	/* only root can mount */
	if (!suser())
		return -EPERM;

	/* remount ? */
	if (flags & MS_REMOUNT)
		return do_remount(dir_name, flags);

	/* find file system type */
	fs = get_fs_type(type);
	if (!fs)
		return -ENODEV;

	/* if filesystem requires dev, find it */
	if (fs->flags & FS_REQUIRES_DEV) {
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
	} else {
		dev = get_unnamed_dev();
		if (!dev)
			return -EMFILE;
	}

	return do_mount(dev, dev_name, dir_name, type, flags, data);
}

/*
 * Mount root file system.
 */
int do_mount_root(dev_t dev, const char *dev_name)
{
	struct file_system_type *fs;
	struct super_block *sb;
	int ret;

	/* try all file systems */
	for (fs = file_systems; fs; fs = fs->next) {
		/* test only dev file systems */
		if (!(fs->flags & FS_REQUIRES_DEV))
			continue;

		/* read super block */
		sb = read_super(dev, fs->name, MS_RDONLY, NULL, 1);
		if (sb)
			goto found;
	}

	return -EINVAL;
found:
	/* set current task */
	current_task->fs->pwd = dget(sb->s_root);
	current_task->fs->root = dget(sb->s_root);

	/* add mounted file system */
	ret = add_vfs_mount(dev, dev_name, "/", 0, sb);
	if (ret) {
		kfree(sb);
		return ret;
	}

	return ret;
}

/*
 * Unset mount.
 */
static int d_umount(struct super_block *sb)
{
	struct dentry *root = sb->s_root;
	struct dentry *covered = root->d_covers;

	/* still busy */
	if (root->d_count != 1 || root->d_inode->i_state)
		return -EBUSY;

	/* unset mount point */
	sb->s_root = NULL;
	if (covered != root) {
		root->d_covers = root;
		covered->d_mounts = covered;
		dput(covered);
	}

	/* release root dentry */
	dput(root);
	return 0;
}

/*
 * Unmount a file system.
 */
static int do_umount(dev_t dev, int flags)
{
	struct super_block *sb;
	int ret;

	/* unused flags */
	UNUSED(flags);

	/* get super block */
	sb = get_super(dev);
	if (!sb || !sb->s_root)
		return -ENOENT;

	/* shrink dentries cache */
	shrink_dcache_sb(sb);

	/* sync device on disk */
	sync_dev(sb->s_dev);

	/* unset mount */
	ret = d_umount(sb);
	if (ret)
		return ret;

	/* remove mounted file system */
	del_vfs_mount(sb);

	/* release super block */
	if (sb->s_op && sb->s_op->put_super)
		sb->s_op->put_super(sb);

	sb->s_dev = 0;
	return 0;
}

/*
 * Unmount a device.
 */
static int umount_dev(dev_t dev, int flags)
{
	int ret;

	/* unmount */
	ret = do_umount(dev, flags);
	if (ret)
		return ret;

	/* release unnamed device */
	put_unnamed_dev(dev);

	return 0;
}

/*
 * Umount2 system call.
 */
int sys_umount2(const char *target, int flags)
{
	struct super_block *sb;
	struct dentry *dentry;
	struct inode *inode;

	/* only root can umount */
	if (!suser())
		return -EPERM;

	/* resolve path */
	dentry = namei(AT_FDCWD, target, 1);
	if (IS_ERR(dentry))
		return PTR_ERR(dentry);

	/* get inode, superblock and dev */
	inode = dentry->d_inode;
	sb = inode->i_sb;

	/* target inode must be a root inode */
	if (!sb || inode != sb->s_root->d_inode) {
		dput(dentry);
		return -EINVAL;
	}

	/* release dentry */
	dput(dentry);

	/* unmount device */
	return umount_dev(sb->s_dev, flags);
}

/*
 * Umount system call.
 */
int sys_umount(const char *target)
{
	return sys_umount2(target, 0);
}