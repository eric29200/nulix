#include <fs/devpts_fs.h>
#include <stderr.h>
#include <fcntl.h>

/*
 * Get directory entries system call.
 */
int devpts_getdents64(struct file *filp, void *dirp, size_t count)
{
	struct devpts_entry *de_dir, *de;
	struct dirent64 *dirent;
	struct list_head *pos;
	int ret, entries_size;
	size_t i;

	/* get devpts entry */
	de_dir = (struct devpts_entry *) filp->f_inode->u.generic_i;
	if (!S_ISDIR(de_dir->mode))
		return -EINVAL;

	/* init directory entry */
	dirent = (struct dirent64 *) dirp;
	entries_size = 0;

	/* add "." entry */
	if (filp->f_pos == 0) {
		/* fill in directory entry */
		ret = filldir(dirent, ".", 1, filp->f_inode->i_ino, count);
		if (ret)
			return entries_size;

		/* go to next entry */
		count -= dirent->d_reclen;
		entries_size += dirent->d_reclen;
		dirent = (struct dirent64 *) ((void *) dirent + dirent->d_reclen);
		filp->f_pos++;
	}

	/* add ".." entry */
	if (filp->f_pos == 1) {
		/* fill in directory entry */
		ret = filldir(dirent, "..", 2, de_dir->parent ? de_dir->parent->ino : de_dir->ino, count);
		if (ret)
			return entries_size;

		/* go to next entry */
		count -= dirent->d_reclen;
		entries_size += dirent->d_reclen;
		dirent = (struct dirent64 *) ((void *) dirent + dirent->d_reclen);
		filp->f_pos++;
	}

	/* walk through all entries */
	i = 2;
	list_for_each(pos, &de_dir->u.dir.entries) {
		de = list_entry(pos, struct devpts_entry, list);

		/* skip entries before offset */
		if (filp->f_pos > i++)
			continue;

		/* fill in directory entry */
		ret = filldir(dirent, de->name, de->name_len, de->ino, count);
		if (ret)
			return entries_size;

		/* go to next entry */
		filp->f_pos++;
		count -= dirent->d_reclen;
		entries_size += dirent->d_reclen;
		dirent = (struct dirent64 *) ((char *) dirent + dirent->d_reclen);
	}

	return entries_size;
}