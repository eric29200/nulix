#include <fs/fs.h>
#include <proc/sched.h>
#include <mm/mm.h>
#include <stdio.h>
#include <stderr.h>
#include <fcntl.h>
#include <string.h>

#define ACC_MODE(x) ("\000\004\002\006"[(x)&O_ACCMODE])

/*
 * Check permission.
 */
int permission(struct inode *inode, int mask)
{
	int mode = inode->i_mode;

	/* read only filesystem */
	if ((mask & S_IWOTH) && IS_RDONLY(inode) && (S_ISREG(mode) || S_ISDIR(mode) || S_ISLNK(mode)))
		return -EROFS;

	/* root */
	if (suser())
		return 0;

	/* user or group */
	if (current_task->fsuid == inode->i_uid)
		mode >>= 6;
	else if (task_in_group(current_task, inode->i_gid))
		mode >>= 3;

	return (mode & mask & S_IRWXO) == mask ? 0 : -EACCES;
}

/*
 * Follow a link.
 */
static struct dentry *do_follow_link(struct dentry *base, struct dentry *dentry)
{
	struct inode *inode = dentry->d_inode;
	struct dentry *res;

	if (inode && inode->i_op && inode->i_op->follow_link) {
		res = inode->i_op->follow_link(inode, base);
		base = dentry;
		dentry = res;
	}

	dput(base);
	return dentry;
}

/*
 * Reserved lookup.
 */
static struct dentry *reserved_lookup(struct dentry *parent, struct qstr *name)
{
	struct dentry *res = NULL;

	if (name->name[0] == '.') {
		switch (name->len) {
			case 1:
				res = parent;
				break;
			case 2:
				if (parent == current_task->fs->root)
					res = parent;
				else
					res = parent->d_covers->d_parent;

				break;
			default:
				break;
		}
	}

	return res;
}

/*
 * Cached lookup.
 */
static struct dentry *cached_lookup(struct dentry *parent, struct qstr *name)
{
	return d_lookup(parent, name);
}

/*
 * Real lookup.
 */
static struct dentry *real_lookup(struct dentry *parent, struct qstr *name)
{
	struct inode *dir = parent->d_inode;
	struct dentry *res;
	int ret;

	/* check cache first */
	res = d_lookup(parent, name);
	if (res)
		return res;

	/* allocate a new dentry */
	res = d_alloc(parent, name);
	if (!res)
		return ERR_PTR(-ENOMEM);

	/* lookup */
	res->d_count++;
	ret = dir->i_op->lookup(dir, res);
	res->d_count--;
	if (ret) {
		d_free(res);
		res = ERR_PTR(ret);
	}

	return res;
}

/*
 * Lookup a file.
 */
static struct dentry *lookup(struct dentry *dir, struct qstr *name)
{
	struct dentry *res;

	/* reserved lookup */
	res = reserved_lookup(dir, name);
	if (res)
		goto out;

	/* cached lookup */
	res = cached_lookup(dir, name);
	if (res)
		goto out;

	/* lookup */
	res = real_lookup(dir, name);
out:
	if (!IS_ERR(res))
		res = dget(res->d_mounts);

	return res;
}

/*
 * Resolve a path.
 */
struct dentry *lookup_dentry(int dirfd, struct dentry *base, const char *pathname, int follow_link)
{
	struct dentry *dentry;
	struct inode *inode;
	struct file *filp;
	struct qstr this;
	char follow, c;
	int ret;

	/* use directly dir fd */
	if (dirfd >= 0 && (!pathname || *pathname == 0)) {
		filp = fget(dirfd);
		if (!filp)
			return ERR_PTR(-EINVAL);

		inode = filp->f_inode;
		inode->i_count++;
		fput(filp);
		return d_alloc_root(inode);
	}

	/* absolute or relative path */
	if (*pathname == '/') {
		if (base)
			dput(base);

		base = dget(current_task->fs->root);
		do {
			pathname++;
		} while (*pathname == '/');
	} else if (base) {
		base = base;
	} else if (dirfd == AT_FDCWD) {
		base = dget(current_task->fs->pwd);
	} else if (dirfd >= 0 && dirfd < NR_OPEN && current_task->files->filp[dirfd]) {
		base = d_alloc_root(current_task->files->filp[dirfd]->f_inode);
	}

	/* no base */
	if (!base)
		return ERR_PTR(-ENOENT);

	/* end of resolution */
	if (!*pathname)
		return base;

	for (;;) {
		/* no such entry */
		dentry = ERR_PTR(-ENOENT);
		inode = base->d_inode;
		if (!inode)
			break;

		/* must be a directory */
		dentry = ERR_PTR(-ENOTDIR);
		if (!inode->i_op || !inode->i_op->lookup)
			break;

		/* check permissions */
		ret = permission(inode, MAY_EXEC);
		dentry = ERR_PTR(ret);
 		if (ret)
			break;

		/* compute next path name */
		this.name = (char *) pathname;
		this.len = 0;
		c = *pathname;
		this.hash = init_name_hash();
		do {
			this.len++;
			pathname++;
			this.hash = partial_name_hash(c, this.hash);
			c = *pathname;
		} while (c && c != '/');
		this.hash = end_name_hash(this.hash);

		/* remove trailing slashes ? */
		follow = follow_link;
		if (c) {
			follow |= c;
			do {
				c = *++pathname;
			} while (c == '/');
		}

		/* lookup */
		dentry = lookup(base, &this);
		if (IS_ERR(dentry))
			break;

		/* end */
		if (!follow)
			break;

		/* follow link */
		base = do_follow_link(base, dentry);
		if (c && !IS_ERR(base))
			continue;

		return base;
	}

	dput(base);
	return dentry;
}

/*
 * Resolve a path.
 */
struct dentry *namei(int dirfd, const char *pathname, int follow_link)
{
	struct dentry *dentry;

	/* resolve path */
	dentry = lookup_dentry(dirfd, NULL, pathname, follow_link);
	if (IS_ERR(dentry))
		return dentry;

	/* no such entry */
	if (!dentry->d_inode) {
		dput(dentry);
		return ERR_PTR(-ENOENT);
	}

	return dentry;
}

/*
 * Resolve and open a path name.
 */
struct dentry *open_namei(int dirfd, const char *pathname, int flags, mode_t mode)
{
	struct inode *dir, *inode;
	struct dentry *dentry;
	int acc_mode, ret;

	/* set mode (needed if new file is created) */
	mode &= S_IALLUGO & ~current_task->fs->umask;
	mode |= S_IFREG;

	/* resolve path */
	dentry = lookup_dentry(dirfd, NULL, pathname, 1);
	if (IS_ERR(dentry))
		return dentry;

	/* create file if needed */
	acc_mode = ACC_MODE(flags);
	if (flags & O_CREAT) {
		dir = dentry->d_parent->d_inode;

		if (dentry->d_inode) {
			ret = 0;
			if (flags & O_EXCL)
				ret = -EEXIST;
		} else if (IS_RDONLY(dir)) {
			ret = -EROFS;
		} else if (!dir->i_op || !dir->i_op->create) {
			ret = -EACCES;
		} else {
			ret = permission(dir, MAY_WRITE | MAY_EXEC);
			if (ret == 0) {
				ret = dir->i_op->create(dir, dentry, mode);
				acc_mode = 0;
			}
		}

		if (ret)
			goto err;
	}

	/* no such entry */
	ret = -ENOENT;
	inode = dentry->d_inode;
	if (!inode)
		goto err;

	/* can't open a directory in write mode */
	ret = -EISDIR;
	if (S_ISDIR(inode->i_mode) && (flags & FMODE_WRITE))
		goto err;

	/* check permissions */
	ret = permission(inode, acc_mode);
	if (ret)
		goto err;

	/* O_DIRECTORY : fails if inode is not a directory */
	ret = -ENOTDIR;
	if ((flags & O_DIRECTORY) && !S_ISDIR(inode->i_mode))
		goto err;

	/* truncate file */
	if (flags & O_TRUNC) {
		/* check permissions */
		ret = permission(inode, MAY_WRITE);
		if (ret)
			goto err;

		ret = do_truncate(inode, 0);
		if (ret)
			goto err;
	}

	return dentry;
err:
	dput(dentry);
	return ERR_PTR(ret);
}

/*
 * Make dir system call.
 */
static int do_mkdir(int dirfd, const char *pathname, mode_t mode)
{
	struct dentry *dentry;
	struct inode *dir;
	int ret;

	/* resolve path */
	dentry = lookup_dentry(dirfd, NULL, pathname, 1);
	if (IS_ERR(dentry))
		return PTR_ERR(dentry);

	/* get directory */
	dir = dentry->d_parent->d_inode;

	/* already exists */
	ret = -EEXIST;
	if (dentry->d_inode)
		goto out;

	/* read only filesystem */
	ret = -EROFS;
	if (IS_RDONLY(dir))
		goto out;

	/* check permissions */
	ret = permission(dir, MAY_WRITE | MAY_EXEC);
	if (ret)
		goto out;

	/* mkdir not implemented */
	ret = -EPERM;
	if (!dir->i_op || !dir->i_op->mkdir)
		goto out;

	/* make directory */
	ret = dir->i_op->mkdir(dir, dentry, mode);
out:
	dput(dentry);
	return ret;
}

/*
 * Mkdir system call.
 */
int sys_mkdir(const char *filename, mode_t mode)
{
	return do_mkdir(AT_FDCWD, filename, mode);
}

/*
 * Mkdirat system call.
 */
int sys_mkdirat(int dirfd, const char *pathname, mode_t mode)
{
	return do_mkdir(dirfd, pathname, mode);
}

/*
 * Link system call (make a new name for a file = hard link).
 */
static int do_link(int olddirfd, const char *oldpath, int newdirfd, const char *newpath)
{
	struct dentry *old_dentry, *new_dentry;
	struct inode *dir, *inode;
	int ret;

	/* resolve old path */
	old_dentry = lookup_dentry(olddirfd, NULL, oldpath, 1);
	if (IS_ERR(old_dentry))
		return PTR_ERR(old_dentry);

	/* resolve new path */
	new_dentry = lookup_dentry(newdirfd, NULL, newpath, 1);
	ret = PTR_ERR(new_dentry);
	if (IS_ERR(new_dentry))
		goto out_release_old;

	/* get directory */
	dir = new_dentry->d_parent->d_inode;

	/* no such entry */
	ret = -ENOENT;
	inode = old_dentry->d_inode;
	if (!inode)
		goto out_release_new;

	/* newpath already exists */
	ret = -EEXIST;
	if (new_dentry->d_inode)
		goto out_release_new;

	/* read only filesystem */
	ret = -EROFS;
	if (IS_RDONLY(dir))
		goto out_release_new;

	/* check permissions */
	ret = permission(dir, MAY_WRITE | MAY_EXEC);
	if (ret)
		goto out_release_new;

	/* link not implemented */
	ret = -EPERM;
	if (!dir->i_op || !dir->i_op->link)
		goto out_release_new;

	/* make link */
	ret = dir->i_op->link(inode, dir, new_dentry);
out_release_new:
	dput(new_dentry);
out_release_old:
	dput(old_dentry);
	return ret;
}

/*
 * Link system call (make a new name for a file = hard link).
 */
int sys_link(const char *oldpath, const char *newpath)
{
	return do_link(AT_FDCWD, oldpath, AT_FDCWD, newpath);
}

/*
 * Linkat system call.
 */
int sys_linkat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath, int flags)
{
	UNUSED(flags);
	return do_link(olddirfd, oldpath, newdirfd, newpath);
}

/*
 * Symbolic link system call.
 */
static int do_symlink(const char *target, int newdirfd, const char *linkpath)
{
	struct dentry *dentry;
	struct inode *dir;
	int ret;

	/* resolve path */
	dentry = lookup_dentry(newdirfd, NULL, linkpath, 0);
	if (IS_ERR(dentry))
		return PTR_ERR(dentry);

	/* get directory */
	dir = dentry->d_parent->d_inode;

	/* linkpath already exists */
	ret = -EEXIST;
	if (dentry->d_inode)
		goto out;

	/* read only filesystem */
	ret = -EROFS;
	if (IS_RDONLY(dir))
		goto out;

	/* check permissions */
	ret = permission(dir, MAY_WRITE | MAY_EXEC);
	if (ret)
		goto out;

	/* symlink not implemented */
	ret = -EPERM;
	if (!dir->i_op || !dir->i_op->symlink)
		goto out;

	/* make symbolic link */
	ret = dir->i_op->symlink(dir, dentry, target);
out:
	dput(dentry);
	return ret;
}

/*
 * Create symbolic link system call.
 */
int sys_symlink(const char *target, const char *linkpath)
{
	return do_symlink(target, AT_FDCWD, linkpath);
}

/*
 * Create symbolic link system call.
 */
int sys_symlinkat(const char *target, int newdirfd, const char *linkpath)
{
	return do_symlink(target, newdirfd, linkpath);
}

/*
 * Rmdir system call (remove a directory).
 */
static int do_rmdir(int dirfd, const char *pathname)
{
	struct dentry *dentry;
	struct inode *dir;
	int ret;

	/* resolve path */
	dentry = lookup_dentry(dirfd, NULL, pathname, 0);
	if (IS_ERR(dentry))
		return PTR_ERR(dentry);

	/* get directory */
	dir = dentry->d_parent->d_inode;

	/* no such entry */
	ret = -ENOENT;
	if (!dentry->d_inode)
		goto out;

	/* read only filesystem */
	ret = -EROFS;
	if (IS_RDONLY(dir))
		goto out;

	/* check permissions */
	ret = permission(dir, MAY_WRITE | MAY_EXEC);
	if (ret)
		goto out;

	/* rmdir not implemented */
	ret = -EPERM;
	if (!dir->i_op || !dir->i_op->rmdir)
		goto out;

	/* remove directory */
	ret = dir->i_op->rmdir(dir, dentry);
out:
	dput(dentry);
	return ret;
}

/*
 * Rmdir system call.
 */
int sys_rmdir(const char *pathname)
{
	return do_rmdir(AT_FDCWD, pathname);
}

/*
 * Unlink system call (remove a file).
 */
static int do_unlink(int dirfd, const char *pathname)
{
	struct dentry *dentry;
	struct inode *dir;
	int ret;

	/* resolve path */
	dentry = lookup_dentry(dirfd, NULL, pathname, 0);
	if (IS_ERR(dentry))
		return PTR_ERR(dentry);

	/* get directory */
	dir = dentry->d_parent->d_inode;

	/* no such entry */
	ret = -ENOENT;
	if (!dentry->d_inode)
		goto out;

	/* read only filesystem */
	ret = -EROFS;
	if (IS_RDONLY(dir))
		goto out;

	/* check permissions */
	ret = permission(dir, MAY_WRITE | MAY_EXEC);
	if (ret)
		goto out;

	/* unlink not implemented */
	ret = -EPERM;
	if (!dir->i_op || !dir->i_op->unlink)
		goto out;

	/* unlink */
	ret = dir->i_op->unlink(dir, dentry);
out:
	dput(dentry);
	return ret;
}

/*
 * Unlink system call.
 */
int sys_unlink(const char *pathname)
{
	return do_unlink(AT_FDCWD, pathname);
}

/*
 * Unlink at system call.
 */
int sys_unlinkat(int dirfd, const char *pathname, int flags)
{
	if (flags & AT_REMOVEDIR)
		return do_rmdir(dirfd, pathname);

	return do_unlink(dirfd, pathname);
}

/*
 * Read value of a symbolic link.
 */
static ssize_t do_readlink(int dirfd, const char *pathname, char *buf, size_t bufsize)
{
	struct dentry *dentry;
	struct inode *inode;
	int ret;

	/* resolve path */
	dentry = namei(dirfd, pathname, 0);
	if (IS_ERR(dentry))
		return PTR_ERR(dentry);

	/* get inode */
	inode = dentry->d_inode;

	/* readlink not implemented */
	ret = -EPERM;
	if (!inode->i_op || !inode->i_op->readlink)
		goto out;

	/* read link */
	ret = inode->i_op->readlink(inode, buf, bufsize);
out:
	dput(dentry);
	return ret;
}

/*
 * Read a symbolic link system call.
 */
ssize_t sys_readlink(const char *pathname, char *buf, size_t bufsize)
{
	return do_readlink(AT_FDCWD, pathname, buf, bufsize);
}

/*
 * Read a symbolic link system call.
 */
ssize_t sys_readlinkat(int dirfd, const char *pathname, char *buf, size_t bufsize)
{
	return do_readlink(dirfd, pathname, buf, bufsize);
}

/*
 * Rename a file.
 */
static int do_rename(int olddirfd, const char *oldpath, int newdirfd, const char *newpath, unsigned int flags)
{
	struct dentry * old_dentry, *new_dentry;
	struct inode *old_dir, *new_dir;
	int ret;

	/* resolve old path */
	old_dentry = lookup_dentry(olddirfd, NULL, oldpath, 0);
	if (IS_ERR(old_dentry))
		return PTR_ERR(old_dentry);

	/* resolve new path */
	new_dentry = lookup_dentry(newdirfd, NULL, newpath, 0);
	ret = PTR_ERR(new_dentry);
	if (IS_ERR(new_dentry))
		goto out_release_old;

	/* get directories */
	new_dir = new_dentry->d_parent->d_inode;
	old_dir = old_dentry->d_parent->d_inode;

	/* no such entry */
	ret = -ENOENT;
	if (!old_dentry->d_inode)
		goto out_release_new;

	/* check permissions */
	ret = permission(old_dir, MAY_WRITE | MAY_EXEC);
	if (ret)
		goto out_release_new;
	ret = permission(new_dir,MAY_WRITE | MAY_EXEC);
	if (ret)
		goto out_release_new;

	/* disallow moves to another device */
	ret = -EXDEV;
	if (new_dir->i_sb != old_dir->i_sb)
		goto out_release_new;

	/* read only filesystem */
	ret = -EROFS;
	if (IS_RDONLY(new_dir) || IS_RDONLY(old_dir))
		goto out_release_new;

	/* rename not implemented */
	ret = -EPERM;
	if (!old_dir->i_op || !old_dir->i_op->rename)
		goto out_release_new;

	/* no replace */
	ret = -EEXIST;
	if ((flags & RENAME_NOREPLACE) && new_dentry->d_inode && S_ISDIR(new_dentry->d_inode->i_mode))
		goto out_release_new;

	/* rename */
	ret = old_dir->i_op->rename(old_dir, old_dentry, new_dir, new_dentry);
out_release_new:
	dput(new_dentry);
out_release_old:
	dput(old_dentry);
	return ret;
}

/*
 * Rename system call.
 */
int sys_rename(const char *oldpath, const char *newpath)
{
	return do_rename(AT_FDCWD, oldpath, AT_FDCWD, newpath, 0);
}

/*
 * Rename at system call.
 */
int sys_renameat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath)
{
	return do_rename(olddirfd, oldpath, newdirfd, newpath, 0);
}

/*
 * Rename at system call.
 */
int sys_renameat2(int olddirfd, const char *oldpath, int newdirfd, const char *newpath, unsigned int flags)
{
	return do_rename(olddirfd, oldpath, newdirfd, newpath, flags);
}

/*
 * Mknod system call.
 */
static int do_mknod(int dirfd, const char *pathname, mode_t mode, dev_t dev)
{
	struct dentry *dentry;
	struct inode *dir;
	int ret;

	/* resolve path */
	dentry = lookup_dentry(dirfd, NULL, pathname, 1);
	if (IS_ERR(dentry))
		return PTR_ERR(dentry);

	/* get directory */
	dir = dentry->d_parent->d_inode;

	/* already exists */
	ret = -EEXIST;
	if (dentry->d_inode)
		goto out;

	/* read only filesystem */
	ret = -EROFS;
	if (IS_RDONLY(dir))
		goto out;

	/* check permissions */
	ret = permission(dir, MAY_WRITE | MAY_EXEC);
	if (ret)
		goto out;

	/* mknod not implemented */
	ret = -EPERM;
	if (!dir->i_op || !dir->i_op->mknod)
		goto out;

	/* make node */
	ret = dir->i_op->mknod(dir, dentry, mode, dev);
out:
	dput(dentry);
	return ret;
}

/*
 * Mknod system call.
 */
int sys_mknod(const char *pathname, mode_t mode, dev_t dev)
{
	return do_mknod(AT_FDCWD, pathname, mode, dev);
}
