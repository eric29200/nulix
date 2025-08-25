#include <fs/tmp_fs.h>
#include <proc/sched.h>
#include <mm/highmem.h>
#include <stderr.h>
#include <fcntl.h>

/*
 * Test if a name matches a directory entry.
 */
static inline int tmpfs_name_match(const char *name1, size_t len1, const char *name2, size_t max_len)
{
	/* check overflow */
	if (len1 > max_len)
		return 0;

	return strncmp(name1, name2, len1) == 0 && (len1 == max_len || name2[len1] == 0);
}

/*
 * Find an entry inside a directory.
 */
static struct tmpfs_dir_entry *tmpfs_find_entry(struct inode *dir, const char *name, int name_len)
{
	int nr_entries_per_page, i;
	struct tmpfs_dir_entry *de;
	struct list_head *pos;
	struct page *page;
	void *kaddr;

	/* check file name length */
	if (name_len <= 0 || name_len > TMPFS_NAME_LEN)
		return NULL;

	/* compute number of entries per page */
	nr_entries_per_page = PAGE_SIZE / sizeof(struct tmpfs_dir_entry);

	/* walk through all pages */
	list_for_each(pos, &dir->u.tmp_i.i_pages) {
		page = list_entry(pos, struct page, list);
		kaddr = kmap(page);

		/* walk through all entries */
		for (i = 0; i < nr_entries_per_page; i++) {
			de = (struct tmpfs_dir_entry *) (kaddr + i * sizeof(struct tmpfs_dir_entry));
			if (tmpfs_name_match(name, name_len, de->d_name, TMPFS_NAME_LEN)) {
				kunmap(page);
				return de;
			}
		}

		kunmap(page);
	}

	return NULL;
}

/*
 * Check if a directory is empty.
 */
static int tmpfs_empty_dir(struct inode *dir)
{
	int nr_entries_per_page, i, first_page;
	struct tmpfs_dir_entry *de;
	struct list_head *pos;
	struct page *page;
	void *kaddr;

	/* check if dir is a directory */
	if (S_ISREG(dir->i_mode))
		return 0;

	/* compute number of entries per page */
	nr_entries_per_page = PAGE_SIZE / sizeof(struct tmpfs_dir_entry);

	/* walk through all pages */
	first_page = 1;
	list_for_each(pos, &dir->u.tmp_i.i_pages) {
		page = list_entry(pos, struct page, list);
		kaddr = kmap(page);

		/* walk through all entries */
		for (i = 0; i < nr_entries_per_page; i++) {
			de = (struct tmpfs_dir_entry *) (kaddr + i * sizeof(struct tmpfs_dir_entry));

			/* skip first 2 entries "." and ".." */
			if (first_page && i < 2)
				continue;

			/* found an entry : directory is not empty */
			if (de->d_inode) {
				kunmap(page);
				return 0;
			}
		}

		kunmap(page);
		first_page = 0;
	}

	return 1;
}

/*
 * Add a directory entry.
 */
int tmpfs_add_entry(struct inode *dir, const char *name, int name_len, struct inode *inode)
{
	int nr_entries_per_page, i, ret;
	struct tmpfs_dir_entry *de;
	struct list_head *pos;
	struct page *page;
	void *kaddr;

	/* check file name */
	if (name_len <= 0 || name_len > TMPFS_NAME_LEN)
		return -EINVAL;

	/* compute number of entries per page */
	nr_entries_per_page = PAGE_SIZE / sizeof(struct tmpfs_dir_entry);

retry:
	/* walk through all pages */
	list_for_each(pos, &dir->u.tmp_i.i_pages) {
		page = list_entry(pos, struct page, list);
		kaddr = kmap(page);

		/* walk through all entries */
		for (i = 0; i < nr_entries_per_page; i++) {
			de = (struct tmpfs_dir_entry *) (kaddr + i * sizeof(struct tmpfs_dir_entry));
			if (!de->d_inode) {
				kunmap(page);
				goto found;
			}
		}

		kunmap(page);
	}

	/* no free directory entry : grow directory */
	ret = tmpfs_inode_grow_size(dir, dir->i_size + PAGE_SIZE);
	if (ret)
		return ret;

	goto retry;
found:
	/* set new directory entry */
	de->d_inode = inode->i_ino;
	memset(de->d_name, 0, TMPFS_NAME_LEN);
	memcpy(de->d_name, name, name_len);

	/* update parent directory */
	dir->i_mtime = dir->i_ctime = CURRENT_TIME;

	return 0;
}

/*
 * Directory lookup.
 */
int tmpfs_lookup(struct inode *dir, const char *name, size_t name_len, struct inode **res_inode)
{
	struct tmpfs_dir_entry *de;

	/* dir must be a directory */
	if (!dir)
		return -ENOENT;
	if (!S_ISDIR(dir->i_mode)) {
		iput(dir);
		return -ENOENT;
	}

	/* find entry */
	de = tmpfs_find_entry(dir, name, name_len);
	if (!de) {
		iput(dir);
		return -ENOENT;
	}

	/* get inode */
	*res_inode = iget(dir->i_sb, de->d_inode);
	if (!*res_inode) {
		iput(dir);
		return -EACCES;
	}

	iput(dir);
	return 0;
}

/*
 * Create a file in a directory.
 */
int tmpfs_create(struct inode *dir, const char *name, size_t name_len, mode_t mode, struct inode **res_inode)
{
	struct inode *inode;
	int ret;

	/* check directory */
	*res_inode = NULL;
	if (!dir)
		return -ENOENT;

	/* check if file already exists */
	dir->i_count++;
	if (tmpfs_find_entry(dir, name, name_len)) {
		iput(dir);
		return -EEXIST;
	}

	/* create a new inode */
	inode = tmpfs_new_inode(dir->i_sb, mode, 0);
	if (!inode) {
		iput(dir);
		return -ENOSPC;
	}

	/* add new entry to dir */
	ret = tmpfs_add_entry(dir, name, name_len, inode);
	if (ret) {
		inode->i_nlinks--;
		iput(inode);
		iput(dir);
		return ret;
	}

	/* set result inode */
	*res_inode = inode;

	/* release directory */
	iput(dir);

	return 0;
}

/*
 * Make a new name for a file = hard link.
 */
int tmpfs_link(struct inode *old_inode, struct inode *dir, const char *name, size_t name_len)
{
	struct tmpfs_dir_entry *de;
	int ret;

	/* check if new file exists */
	de = tmpfs_find_entry(dir, name, name_len);
	if (de) {
		iput(old_inode);
		iput(dir);
		return -EEXIST;
	}

	/* add entry */
	ret = tmpfs_add_entry(dir, name, name_len, old_inode);
	if (ret) {
		iput(old_inode);
		iput(dir);
		return ret;
	}

	/* update old inode */
	old_inode->i_ctime = CURRENT_TIME;
	old_inode->i_nlinks++;
	mark_inode_dirty(old_inode);

	/* release inodes */
	iput(old_inode);
	iput(dir);

	return 0;
}

/*
 * Remove a file.
 */
int tmpfs_unlink(struct inode *dir, const char *name, size_t name_len)
{
	struct tmpfs_dir_entry *de;
	struct inode *inode;

	/* get directory entry */
	de = tmpfs_find_entry(dir, name, name_len);
	if (!de) {
		iput(dir);
		return -ENOENT;
	}

	/* get inode */
	inode = iget(dir->i_sb, de->d_inode);
	if (!inode) {
		iput(dir);
		return -ENOENT;
	}

	/* remove regular files only */
	if (S_ISDIR(inode->i_mode)) {
		iput(inode);
		iput(dir);
		return -EPERM;
	}

	/* reset directory entry */
	memset(de, 0, sizeof(struct tmpfs_dir_entry));

	/* update directory */
	dir->i_ctime = dir->i_mtime = CURRENT_TIME;
	mark_inode_dirty(dir);

	/* update inode */
	inode->i_ctime = dir->i_ctime;
	inode->i_nlinks--;
	mark_inode_dirty(inode);

	/* release inode */
	iput(inode);
	iput(dir);

	return 0;
}

/*
 * Create a symbolic link.
 */
int tmpfs_symlink(struct inode *dir, const char *name, size_t name_len, const char *target)
{
	struct tmpfs_dir_entry *de;
	struct inode *inode;
	struct page *page;
	char *kaddr;
	int ret, i;

	/* create a new inode */
	inode = tmpfs_new_inode(dir->i_sb, S_IFLNK | (0777 & ~current_task->fs->umask), 0);
	if (!inode) {
		iput(dir);
		return -ENOSPC;
	}

	/* create first page */
	ret = tmpfs_inode_grow_size(inode, PAGE_SIZE);
	if (ret) {
		inode->i_nlinks = 0;
		iput(inode);
		iput(dir);
		return -ENOMEM;
	}

	/* get first page */
	page = list_first_entry(&inode->u.tmp_i.i_pages, struct page, list);
	kaddr = kmap(page);

	/* write file name on first page */
	for (i = 0; target[i] && i < PAGE_SIZE - 1; i++)
		kaddr[i] = target[i];
	kaddr[i] = 0;
	kunmap(page);

	/* check if file exists */
	de = tmpfs_find_entry(dir, name, name_len);
	if (de) {
		inode->i_nlinks = 0;
		iput(inode);
		iput(dir);
		return -EEXIST;
	}

	/* add entry */
	ret = tmpfs_add_entry(dir, name, name_len, inode);
	if (ret) {
		inode->i_nlinks = 0;
		iput(inode);
		iput(dir);
		return ret;
	}

	/* release inode */
	iput(inode);
	iput(dir);

	return 0;
}

/*
 * Create a directory.
 */
int tmpfs_mkdir(struct inode *dir, const char *name, size_t name_len, mode_t mode)
{
	struct tmpfs_dir_entry *de;
	struct inode *inode;
	int ret;

	/* check if file exists */
	de = tmpfs_find_entry(dir, name, name_len);
	if (de) {
		iput(dir);
		return -EEXIST;
	}

	/* allocate a new inode */
	inode = tmpfs_new_inode(dir->i_sb, S_IFDIR | (mode & ~current_task->fs->umask & 0777), 0);
	if (!inode) {
		iput(dir);
		return -ENOMEM;
	}

	/* create "." entry in root directory */
	ret = tmpfs_add_entry(inode, ".", 1, inode);
	if (ret)
		goto err;

	/* create ".." entry in root directory */
	ret = tmpfs_add_entry(inode, "..", 2, dir);
	if (ret)
		goto err;

	/* add entry to parent dir */
	ret = tmpfs_add_entry(dir, name, name_len, inode);
	if (ret) 
		goto err;

	/* update directory links and mark it dirty */
	dir->i_nlinks++;
	mark_inode_dirty(dir);

	/* release inode */
	iput(dir);
	iput(inode);

	return 0;
err:
	inode->i_nlinks = 0;
	iput(inode);
	iput(dir);
	return ret;
}

/*
 * Remove a directory.
 */
int tmpfs_rmdir(struct inode *dir, const char *name, size_t name_len)
{
	struct tmpfs_dir_entry *de;
	struct inode *inode;

	/* check if file exists */
	de = tmpfs_find_entry(dir, name, name_len);
	if (!de) {
		iput(dir);
		return -ENOENT;
	}

	/* get inode */
	inode = iget(dir->i_sb, de->d_inode);
	if (!inode) {
		iput(dir);
		return -ENOENT;
	}

	/* remove directories only and do not allow to remove '.' */
	if (!S_ISDIR(inode->i_mode) || inode->i_ino == dir->i_ino) {
		iput(inode);
		iput(dir);
		return -EPERM;
	}

	/* directory must be empty */
	if (!tmpfs_empty_dir(inode)) {
		iput(inode);
		iput(dir);
		return -EPERM;
	}

	/* reset entry */
	memset(de, 0, sizeof(struct tmpfs_dir_entry));

	/* update dir */
	dir->i_ctime = dir->i_mtime = CURRENT_TIME;
	dir->i_nlinks--;
	mark_inode_dirty(dir);

	/* update inode */
	inode->i_ctime = dir->i_ctime;
	inode->i_nlinks = 0;
	mark_inode_dirty(inode);

	/* release inode and directory */
	iput(inode);
	iput(dir);

	return 0;
}

/*
 * Rename a file.
 */
int tmpfs_rename(struct inode *old_dir, const char *old_name, size_t old_name_len,
	         struct inode *new_dir, const char *new_name, size_t new_name_len)
{
	struct inode *old_inode = NULL, *new_inode = NULL;
	struct tmpfs_dir_entry *old_de, *new_de;
	int ret;

	/* find old entry */
	old_de = tmpfs_find_entry(old_dir, old_name, old_name_len);
	if (!old_de) {
		ret = -ENOENT;
		goto out;
	}

	/* get old inode */
	old_inode = iget(old_dir->i_sb, old_de->d_inode);
	if (!old_inode) {
		ret = -ENOSPC;
		goto out;
	}

	/* find new entry (if exists) or add new one */
	new_de = tmpfs_find_entry(new_dir, new_name, new_name_len);
	if (new_de) {
		/* get new inode */
		new_inode = iget(new_dir->i_sb, new_de->d_inode);
		if (!new_inode) {
			ret = -ENOSPC;
			goto out;
		}

		/* same inode : exit */
		if (old_inode->i_ino == new_inode->i_ino) {
			ret = 0;
			goto out;
		}

		/* modify new directory entry inode */
		new_de->d_inode = old_inode->i_ino;

		/* update new inode */
		new_inode->i_nlinks--;
		new_inode->i_atime = new_inode->i_mtime = CURRENT_TIME;
		mark_inode_dirty(new_inode);
	} else {
		/* add new entry */
		ret = tmpfs_add_entry(new_dir, new_name, new_name_len, old_inode);
		if (ret)
			goto out;
	}

	/* cancel old directory entry */
	old_de->d_inode = 0;
	memset(old_de->d_name, 0, TMPFS_NAME_LEN);

	/* update old and new directories */
	old_dir->i_atime = old_dir->i_mtime = CURRENT_TIME;
	mark_inode_dirty(old_dir);
	new_dir->i_atime = new_dir->i_mtime = CURRENT_TIME;
	mark_inode_dirty(new_dir);

	ret = 0;
out:
	/* release inodes */
	iput(old_inode);
	iput(new_inode);
	iput(old_dir);
	iput(new_dir);

	return ret;
}

/*
 * Create a node.
 */
int tmpfs_mknod(struct inode *dir, const char *name, size_t name_len, mode_t mode, dev_t dev)
{
	struct inode *inode;
	int ret;

	/* check directory */
	if (!dir)
		return -ENOENT;

	/* check if file already exists */
	dir->i_count++;
	if (tmpfs_find_entry(dir, name, name_len)) {
		iput(dir);
		return -EEXIST;
	}

	/* create a new inode */
	inode = tmpfs_new_inode(dir->i_sb, mode, dev);
	if (!inode) {
		iput(dir);
		return -ENOSPC;
	}

	/* add new entry to dir */
	ret = tmpfs_add_entry(dir, name, name_len, inode);
	if (ret) {
		inode->i_nlinks--;
		iput(inode);
		iput(dir);
		return ret;
	}

	/* release inode (to write it on disk) */
	iput(inode);
	iput(dir);

	return 0;
}
