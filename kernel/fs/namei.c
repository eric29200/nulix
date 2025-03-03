#include <fs/fs.h>
#include <proc/sched.h>
#include <mm/mm.h>
#include <stdio.h>
#include <stderr.h>
#include <fcntl.h>
#include <string.h>

/*
 * Follow a link.
 */
static struct inode *follow_link(struct inode *dir, struct inode *inode, int flags, mode_t mode)
{
	struct inode *res_inode;

	/* not implemented : return inode */
	if (!inode || !inode->i_op || !inode->i_op->follow_link)
		return inode;

	inode->i_op->follow_link(dir, inode, flags, mode, &res_inode);
	return res_inode;
}

/*
 * Lookup a file.
 */
static int lookup(struct inode *dir, const char *name, size_t name_len, struct inode **res_inode)
{
	struct super_block *sb;

	/* reset result inode */
	*res_inode = NULL;

	/* no parent directory */
	if (!dir)
		return -ENOENT;

	/* special cases */
	if (name_len == 2 && name[0] == '.' && name[1] == '.') {
		/* .. in root = root */
		if (dir == current_task->fs->root) {
			*res_inode = dir;
			return 0;
		}

		/* cross mount point */
		sb = dir->i_sb;
		if (dir == sb->s_root_inode) {
			iput(dir);
			dir = sb->s_covered;
			if (!dir)
				return -ENOENT;
			dir->i_ref++;
		}
	}

	/* lookup not implemented */
	if (!dir->i_op || !dir->i_op->lookup)
		return -ENOTDIR;

	/* dir */
	if (!name_len) {
		*res_inode = dir;
		return 0;
	}

	/* real lookup */
	return dir->i_op->lookup(dir, name, name_len, res_inode);
}

/*
 * Resolve a path name to the inode of the top most directory.
 */
static struct inode *dir_namei(int dirfd, struct inode *base, const char *pathname, const char **basename, size_t *basename_len)
{
	struct inode *inode, *tmp;
	const char *name;
	size_t name_len;

	/* absolute or relative path */
	if (*pathname == '/') {
		inode = current_task->fs->root;
		pathname++;
	} else if (base) {
		inode = base;
	} else if (dirfd == AT_FDCWD) {
		inode = current_task->fs->cwd;
	} else if (dirfd >= 0 && dirfd < NR_OPEN && current_task->files->filp[dirfd]) {
		inode = current_task->files->filp[dirfd]->f_inode;
	}

	if (!inode)
		return NULL;

	/* update reference count */
	inode->i_ref++;

	while (1) {
		/* check if inode is a directory */
		if (!S_ISDIR(inode->i_mode)) {
			iput(inode);
			return NULL;
		}

		/* compute next path name */
		name = pathname;
		for (name_len = 0; *pathname && *pathname++ != '/'; name_len++);

		/* end : return current inode */
		if (!*pathname)
			break;

		/* skip empty folder */
		if (!name_len)
			continue;

		/* lookup */
		inode->i_ref++;
		if (lookup(inode, name, name_len, &tmp)) {
			iput(inode);
			return NULL;
		}

		/* follow symbolic links */
		inode = follow_link(inode, tmp, 0, 0);
		if (!inode)
			return NULL;
	}

	*basename = name;
	*basename_len = name_len;
	return inode;
}

/*
 * Resolve a path name to the matching inode.
 */
struct inode *namei(int dirfd, struct inode *base, const char *pathname, int follow_links)
{
	struct inode *dir, *inode;
	const char *basename;
	size_t basename_len;

	/* use directly dir fd */
	if (dirfd >= 0 && (!pathname || *pathname == 0)) {
		if (dirfd >= NR_OPEN || !current_task->files->filp[dirfd])
			return NULL;

		current_task->files->filp[dirfd]->f_inode->i_ref++;
		return current_task->files->filp[dirfd]->f_inode;
	}

	/* find directory */
	dir = dir_namei(dirfd, base, pathname, &basename, &basename_len);
	if (!dir)
		return NULL;

	/* special case : '/' */
	if (!basename_len)
		return dir;

	/* lookup file */
	dir->i_ref++;
	if (lookup(dir, basename, basename_len, &inode)) {
		iput(dir);
		return NULL;
	}

	/* follow symbolic link */
	if (follow_links)
		inode = follow_link(dir, inode, 0, 0);

	iput(dir);
	return inode;
}

/*
 * Resolve and open a path name.
 */
int open_namei(int dirfd, struct inode *base, const char *pathname, int flags, mode_t mode, struct inode **res_inode)
{
	struct inode *dir, *inode;
	const char *basename;
	size_t basename_len;
	int err;

	*res_inode = NULL;

	/* find directory */
	dir = dir_namei(dirfd, base, pathname, &basename, &basename_len);
	if (!dir)
		return -ENOENT;

	/* open a directory */
	if (!basename_len) {
		/* do not allow to create/truncate directories here */
		if (!(flags & (O_ACCMODE | O_CREAT | O_TRUNC))) {
			*res_inode = dir;
			return 0;
		}

		iput(dir);
		return -EISDIR;
	}

	/* set mode (needed if new file is created) */
	mode &= ~current_task->fs->umask & 0777;
	mode |= S_IFREG;

	/* lookup inode */
	dir->i_ref++;
	if (lookup(dir, basename, basename_len, &inode)) {
		/* no such entry */
		if (!(flags & O_CREAT)) {
			iput(dir);
			return -ENOENT;
		}

		/* create not implemented */
		if (!dir->i_op || !dir->i_op->create) {
			iput(dir);
			return -EPERM;
		}

		/* create new inode */
		dir->i_ref++;
		err = dir->i_op->create(dir, basename, basename_len, mode, res_inode);

		/* release directory */
		iput(dir);
		return err;
	}

	/* follow symbolic link */
	*res_inode = follow_link(dir, inode, flags, mode);
	if (!*res_inode)
		return -EACCES;

	/* O_DIRECTORY : fails if inode is not a directory */
	if ((flags & O_DIRECTORY) && !S_ISDIR((*res_inode)->i_mode)) {
		iput(inode);
		return -ENOTDIR;
	}

	/* truncate file */
	if (flags & O_TRUNC) {
		err = do_truncate(*res_inode, 0);
		if (err) {
			iput(inode);
			return err;
		}
	}

	return 0;
}

/*
 * Make dir system call.
 */
static int do_mkdir(int dirfd, const char *pathname, mode_t mode)
{
	const char *basename;
	size_t basename_len;
	struct inode *dir;
	int err;

	/* get parent directory */
	dir = dir_namei(dirfd, NULL, pathname, &basename, &basename_len);
	if (!dir)
		return -ENOENT;

	/* check name length */
	if (!basename_len) {
		iput(dir);
		return -ENOENT;
	}

	/* mkdir not implemented */
	if (!dir->i_op || !dir->i_op->mkdir) {
		iput(dir);
		return -EPERM;
	}

	/* create directory */
	dir->i_ref++;
	err = dir->i_op->mkdir(dir, basename, basename_len, mode);
	iput(dir);

	return err;
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
	struct inode *old_inode, *dir;
	const char *basename;
	size_t basename_len;
	int err;

	/* get old inode */
	old_inode = namei(olddirfd, NULL, oldpath, 1);
	if (!old_inode)
		return -ENOENT;

	/* do not allow to rename directory */
	if (S_ISDIR(old_inode->i_mode)) {
		iput(old_inode);
		return -EPERM;
	}

	/* get directory of new file */
	dir = dir_namei(newdirfd, NULL, newpath, &basename, &basename_len);
	if (!dir) {
		iput(old_inode);
		return -EACCES;
	}

	/* no name to new file */
	if (!basename_len) {
		iput(old_inode);
		iput(dir);
		return -EPERM;
	}

	/* link not implemented */
	if (!dir->i_op || !dir->i_op->link) {
		iput(old_inode);
		iput(dir);
		return -EPERM;
	}

	/* create link */
	dir->i_ref++;
	err = dir->i_op->link(old_inode, dir, basename, basename_len);

	iput(old_inode);
	iput(dir);
	return err;
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
	const char *basename;
	size_t basename_len;
	struct inode *dir;
	int err;

	/* get new parent directory */
	dir = dir_namei(newdirfd, NULL, linkpath, &basename, &basename_len);
	if (!dir)
		return -ENOENT;

	/* check directory name */
	if (!basename_len) {
		iput(dir);
		return -ENOENT;
	}

	/* symlink not implemented */
	if (!dir->i_op || !dir->i_op->symlink) {
		iput(dir);
		return -EPERM;
	}

	/* create symbolic link */
	dir->i_ref++;
	err = dir->i_op->symlink(dir, basename, basename_len, target);

	iput(dir);
	return err;
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
	const char *basename;
	size_t basename_len;
	struct inode *dir;
	int err;

	/* get parent directory */
	dir = dir_namei(dirfd, NULL, pathname, &basename, &basename_len);
	if (!dir)
		return -ENOENT;

	/* check name length */
	if (!basename_len) {
		iput(dir);
		return -ENOENT;
	}

	/* rmdir not implemented */
	if (!dir->i_op || !dir->i_op->rmdir) {
		iput(dir);
		return -EPERM;
	}

	/* remove directory */
	err = dir->i_op->rmdir(dir, basename, basename_len);
	return err;
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
	const char *basename;
	size_t basename_len;
	struct inode *dir;
	int err;

	/* get parent directory */
	dir = dir_namei(dirfd, NULL, pathname, &basename, &basename_len);
	if (!dir)
		return -ENOENT;

	/* check name length */
	if (!basename_len) {
		iput(dir);
		return -ENOENT;
	}

	/* unlink not implemented */
	if (!dir->i_op || !dir->i_op->unlink) {
		iput(dir);
		return -EPERM;
	}

	/* unlink file */
	err = dir->i_op->unlink(dir, basename, basename_len);
	return err;
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
	struct inode *inode;

	/* get inode */
	inode = namei(dirfd, NULL, pathname, 0);
	if (!inode)
		return -ENOENT;

	/* readlink not implemented */
	if (!inode->i_op || !inode->i_op->readlink) {
		iput(inode);
		return -EINVAL;
	}

	/* read link */
	return inode->i_op->readlink(inode, buf, bufsize);
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
	size_t old_basename_len, new_basename_len;
	const char *old_basename, *new_basename;
	struct inode *old_dir, *new_dir, *tmp;
	int ret;

	/* get old directory */
	old_dir = dir_namei(olddirfd, NULL, oldpath, &old_basename, &old_basename_len);
	if (!old_dir)
		return -ENOENT;

	/* do not allow to move '.' and '..' */
	if (!old_basename_len || (old_basename[0] == '.' && (old_basename_len == 1 || (old_basename[1] == '.' && old_basename_len == 2)))) {
		iput(old_dir);
		return -EPERM;
	}

	/* get new directory */
	new_dir = dir_namei(newdirfd, NULL, newpath, &new_basename, &new_basename_len);
	if (!new_dir) {
		iput(old_dir);
		return -ENOENT;
	}

	/* do not allow to move to '.' and '..' */
	if (!new_basename_len || (new_basename[0] == '.' && (new_basename_len == 1 || (new_basename[1] == '.' && new_basename_len == 2)))) {
		iput(old_dir);
		iput(new_dir);
		return -EPERM;
	}

	/* do not allow to move to another device */
	if (old_dir->i_sb != new_dir->i_sb) {
		iput(old_dir);
		iput(new_dir);
		return -EPERM;
	}

	/* rename not implemented */
	if (!old_dir->i_op || !old_dir->i_op->rename) {
		iput(old_dir);
		iput(new_dir);
		return -EPERM;
	}

	/* check if target already exists */
	if (flags & RENAME_NOREPLACE) {
		ret = lookup(new_dir, new_basename, new_basename_len, &tmp);
		if (ret == 0) {
			/* do not allow to replace directory */
			if (S_ISDIR(tmp->i_mode)) {
				iput(tmp);
				iput(old_dir);
				iput(new_dir);
				return -EEXIST;
			}

			/* release inode */
			iput(tmp);
		}
	}

	new_dir->i_ref++;
	return old_dir->i_op->rename(old_dir, old_basename, old_basename_len, new_dir, new_basename, new_basename_len);
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
	const char *basename;
	size_t basename_len;
	struct inode *dir;

	/* get directory */
	dir = dir_namei(dirfd, NULL, pathname, &basename, &basename_len);
	if (!dir)
		return -ENOENT;

	/* mknod not implemented */
	if (!dir->i_op || !dir->i_op->mknod) {
		iput(dir);
		return -EPERM;
	}

	return dir->i_op->mknod(dir, basename, basename_len, mode, dev);
}

/*
 * Mknod system call.
 */
int sys_mknod(const char *pathname, mode_t mode, dev_t dev)
{
	return do_mknod(AT_FDCWD, pathname, mode, dev);
}
