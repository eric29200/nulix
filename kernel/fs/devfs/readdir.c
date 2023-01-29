#include <fs/dev_fs.h>
#include <stderr.h>
#include <fcntl.h>

/*
 * Get directory entries system call.
 */
int devfs_getdents64(struct file_t *filp, void *dirp, size_t count)
{
	struct devfs_entry_t *de_dir, *de;
	struct dirent64_t *dirent;
	struct list_head_t *pos;
	int ret, entries_size;
	size_t i;

	/* get devfs entry */
	de_dir = (struct devfs_entry_t *) filp->f_inode->u.generic_i;
	if (!S_ISDIR(de_dir->mode))
		return -EINVAL;

	/* init directory entry */
	dirent = (struct dirent64_t *) dirp;
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
		dirent = (struct dirent64_t *) ((void *) dirent + dirent->d_reclen);
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
		dirent = (struct dirent64_t *) ((void *) dirent + dirent->d_reclen);
		filp->f_pos++;
	}

	/* walk through all entries */
	i = 2;
	list_for_each(pos, &de_dir->u.dir.entries) {
		de = list_entry(pos, struct devfs_entry_t, list);

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
		dirent = (struct dirent64_t *) ((char *) dirent + dirent->d_reclen);
	}

	return entries_size;
}
