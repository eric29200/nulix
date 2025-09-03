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
int tmpfs_link(struct inode *inode, struct inode *dir, struct dentry *dentry)
{
	struct tmpfs_dir_entry *de;
	int ret;

	/* inode must not be a directory */
	if (S_ISDIR(inode->i_mode))
		return -EPERM;

	/* check if new file exists */
	de = tmpfs_find_entry(dir, dentry->d_name.name, dentry->d_name.len);
	if (de)
		return -EEXIST;

	/* add entry */
	ret = tmpfs_add_entry(dir, dentry->d_name.name, dentry->d_name.len, inode);
	if (ret)
		return ret;

	/* update old inode */
	inode->i_ctime = CURRENT_TIME;
	inode->i_nlinks++;
	mark_inode_dirty(inode);

	/* instantiate dentry */
	inode->i_count++;
	d_instantiate(dentry, inode);

	return 0;
}

/*
 * Remove a file.
 */
int tmpfs_unlink(struct inode *dir, struct dentry *dentry)
{
	struct inode *inode = dentry->d_inode;
	struct tmpfs_dir_entry *de;
	int ret;

	/* get directory entry */
	de = tmpfs_find_entry(dir, dentry->d_name.name, dentry->d_name.len);
	if (!de)
		return -ENOENT;

	/* remove regular files only */
	ret = -EPERM;
	if (S_ISDIR(inode->i_mode))
		goto out;

	/* check inode */
	if (de->d_inode != inode->i_ino)
		goto out;

	/* reset directory entry */
	memset(de, 0, sizeof(struct tmpfs_dir_entry));

	/* update directory */
	dir->i_ctime = dir->i_mtime = CURRENT_TIME;
	mark_inode_dirty(dir);

	/* update inode */
	inode->i_ctime = dir->i_ctime;
	inode->i_nlinks--;
	mark_inode_dirty(inode);

	/* delete dentry */
	d_delete(dentry);
	ret = 0;
out:
	return ret;
}

/*
 * Create a symbolic link.
 */
int tmpfs_symlink(struct inode *dir, struct dentry *dentry, const char *target)
{
	struct tmpfs_dir_entry *de;
	struct inode *inode;
	struct page *page;
	char *kaddr;
	int ret, i;

	/* create a new inode */
	inode = tmpfs_new_inode(dir->i_sb, S_IFLNK | (0777 & ~current_task->fs->umask), 0);
	if (!inode)
		return -ENOSPC;

	/* create first page */
	ret = tmpfs_inode_grow_size(inode, PAGE_SIZE);
	if (ret) {
		inode->i_nlinks = 0;
		iput(inode);
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
	de = tmpfs_find_entry(dir, dentry->d_name.name, dentry->d_name.len);
	if (de) {
		inode->i_nlinks = 0;
		iput(inode);
		return -EEXIST;
	}

	/* add entry */
	ret = tmpfs_add_entry(dir, dentry->d_name.name, dentry->d_name.len, inode);
	if (ret) {
		inode->i_nlinks = 0;
		iput(inode);
		return ret;
	}

	/* instantiate dentry */
	d_instantiate(dentry, inode);

	return 0;
}

/*
 * Create a directory.
 */
int tmpfs_mkdir(struct inode *dir, struct dentry *dentry, mode_t mode)
{
	struct tmpfs_dir_entry *de;
	struct inode *inode;
	int ret;

	/* check if file exists */
	de = tmpfs_find_entry(dir, dentry->d_name.name, dentry->d_name.len);
	if (de)
		return -EEXIST;

	/* allocate a new inode */
	inode = tmpfs_new_inode(dir->i_sb, S_IFDIR | (mode & ~current_task->fs->umask & 0777), 0);
	if (!inode)
		return -ENOSPC;

	/* create "." entry in root directory */
	ret = tmpfs_add_entry(inode, ".", 1, inode);
	if (ret)
		goto err;

	/* create ".." entry in root directory */
	ret = tmpfs_add_entry(inode, "..", 2, dir);
	if (ret)
		goto err;

	/* add entry to parent dir */
	ret = tmpfs_add_entry(dir, dentry->d_name.name, dentry->d_name.len, inode);
	if (ret) 
		goto err;

	/* update directory links and mark it dirty */
	dir->i_nlinks++;
	mark_inode_dirty(dir);

	/* instantiate dentry */
	d_instantiate(dentry, inode);

	return 0;
err:
	inode->i_nlinks = 0;
	iput(inode);
	return ret;
}

/*
 * Remove a directory.
 */
int tmpfs_rmdir(struct inode *dir, struct dentry *dentry)
{
	struct inode *inode = dentry->d_inode;
	struct tmpfs_dir_entry *de;
	int ret;

	/* check if file exists */
	de = tmpfs_find_entry(dir, dentry->d_name.name, dentry->d_name.len);
	if (!de)
		return -ENOENT;

	/* can't remove "." */
	ret = -EPERM;
	if (inode == dir)
		goto out;

	/* must be a directory */
	ret = -ENOTDIR;
	if (!S_ISDIR(inode->i_mode))
		goto out;

	/* check inode */
	ret = -ENOENT;
	if (inode->i_ino != de->d_inode)
		goto out;

	/* directory must be empty */
	ret = -EPERM;
	if (!tmpfs_empty_dir(inode))
		goto out;

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

	/* delete dentry */
	d_delete(dentry);
	ret = 0;
out:
	return ret;
}

/*
 * Rename a file.
 */
int tmpfs_rename(struct inode *old_dir, struct dentry *old_dentry, struct inode *new_dir, struct dentry *new_dentry)
{
	struct inode *old_inode = old_dentry->d_inode, *new_inode = new_dentry->d_inode;
	struct tmpfs_dir_entry *old_de, *new_de;
	int ret;

	/* find old entry */
	ret = -ENOENT;
	old_de = tmpfs_find_entry(old_dir, old_dentry->d_name.name, old_dentry->d_name.len);
	if (!old_de)
		goto out;

	/* check old inode */
	if (old_inode->i_ino != old_de->d_inode)
		goto out;

	/* find new entry (if exists) or add new one */
	new_de = tmpfs_find_entry(new_dir, new_dentry->d_name.name, new_dentry->d_name.len);
	if (new_de) {
		/* check new inode */
		ret = -ENOENT;
		if (!new_inode || new_inode->i_ino != new_de->d_inode)
			goto out;

		/* same inode : exit */
		ret = 0;
		if (old_inode->i_ino == new_inode->i_ino)
			goto out;

		/* modify new directory entry inode */
		new_de->d_inode = old_inode->i_ino;

		/* update new inode */
		new_inode->i_nlinks--;
		new_inode->i_atime = new_inode->i_mtime = CURRENT_TIME;
		mark_inode_dirty(new_inode);
	} else {
		/* add new entry */
		ret = tmpfs_add_entry(new_dir, new_dentry->d_name.name, new_dentry->d_name.len, old_inode);
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

	/* update dcache */
	d_move(old_dentry, new_dentry);
	ret = 0;
out:
	return ret;
}

/*
 * Create a node.
 */
int tmpfs_mknod(struct inode *dir, struct dentry *dentry, mode_t mode, dev_t dev)
{
	struct inode *inode;
	int ret;

	/* check directory */
	if (!dir)
		return -ENOENT;

	/* check if file already exists */
	if (tmpfs_find_entry(dir, dentry->d_name.name, dentry->d_name.len))
		return -EEXIST;

	/* create a new inode */
	inode = tmpfs_new_inode(dir->i_sb, mode, dev);
	if (!inode)
		return -ENOSPC;

	/* add new entry to dir */
	ret = tmpfs_add_entry(dir, dentry->d_name.name, dentry->d_name.len, inode);
	if (ret) {
		inode->i_nlinks--;
		iput(inode);
		return ret;
	}

	/* instantiate dentry */
	d_instantiate(dentry, inode);

	return 0;
}
