#include <fs/devpts_fs.h>
#include <stderr.h>
#include <fcntl.h>

/*
 * Read directory.
 */
int devpts_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	struct devpts_entry *de_dir, *de;
	struct dentry *dentry;
	struct list_head *pos;
	struct inode *inode;
	size_t i;

	/* get dentry */
	dentry = filp->f_dentry;
	if (!dentry)
		return -ENOENT;

	/* get inode */
	inode = dentry->d_inode;
	if (!inode)
		return -ENOENT;

	/* get devpts entry */
	de_dir = (struct devpts_entry *) inode->u.generic_i;
	if (!S_ISDIR(de_dir->mode))
		return -EINVAL;

	/* add "." entry */
	if (filp->f_pos == 0) {
		if (filldir(dirent, ".", 1, 0, inode->i_ino))
			return 0;
		filp->f_pos = 1;
	}
	/* add ".." entry */
	if (filp->f_pos == 1) {
		if (filldir(dirent, "..", 2, 1, inode->i_ino))
			return 0;
		filp->f_pos = 2;
	}

	/* walk through all entries */
	i = 2;
	list_for_each(pos, &de_dir->u.dir.entries) {
		de = list_entry(pos, struct devpts_entry, list);

		/* skip entries before offset */
		if (filp->f_pos > i++)
			continue;

		/* fill in directory entry */
		if (filldir(dirent, de->name, de->name_len, filp->f_pos, de->ino))
			return 0;

		/* go to next entry */
		filp->f_pos++;
	}

	return 0;
}