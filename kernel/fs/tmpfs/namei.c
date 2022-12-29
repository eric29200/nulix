#include <fs/tmp_fs.h>
#include <proc/sched.h>
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
static struct tmpfs_dir_entry_t *tmpfs_find_entry(struct inode_t *dir, const char *name, int name_len)
{
	int nb_entries_per_page, i;
	struct tmpfs_dir_entry_t *de;
	struct list_head_t *pos;
	struct page_t *page;

	/* check file name length */
	if (name_len <= 0 || name_len > TMPFS_NAME_LEN)
		return NULL;
	
	/* compute number of entries per page */
	nb_entries_per_page = PAGE_SIZE / sizeof(struct tmpfs_dir_entry_t);

	/* walk through all pages */
	list_for_each(pos, &dir->u.tmp_i.i_pages) {
		page = list_entry(pos, struct page_t, list);

		/* walk through all entries */
		for (i = 0; i < nb_entries_per_page; i++) {
			de = (struct tmpfs_dir_entry_t *) (PAGE_ADDRESS(page) + i * sizeof(struct tmpfs_dir_entry_t));
			if (tmpfs_name_match(name, name_len, de->d_name, TMPFS_NAME_LEN))
				return de;
		}
	}

	return NULL;
}

/*
 * Check if a directory is empty.
 */
static int tmpfs_empty_dir(struct inode_t *dir)
{
	int nb_entries_per_page, i, first_page;
	struct tmpfs_dir_entry_t *de;
	struct list_head_t *pos;
	struct page_t *page;

	/* check if dir is a directory */
	if (S_ISREG(dir->i_mode))
		return 0;

	/* compute number of entries per page */
	nb_entries_per_page = PAGE_SIZE / sizeof(struct tmpfs_dir_entry_t);

	/* walk through all pages */
	first_page = 1;
	list_for_each(pos, &dir->u.tmp_i.i_pages) {
		page = list_entry(pos, struct page_t, list);

		/* walk through all entries */
		for (i = 0; i < nb_entries_per_page; i++) {
			de = (struct tmpfs_dir_entry_t *) (PAGE_ADDRESS(page) + i * sizeof(struct tmpfs_dir_entry_t));

			/* skip first 2 entries "." and ".." */
			if (first_page && i < 2)
				continue;
			
			/* found an entry : directory is not empty */
			if (de->d_inode)
				return 0;
		}

		first_page = 0;
	}

	return 1;
}

/*
 * Add a directory entry.
 */
int tmpfs_add_entry(struct inode_t *dir, const char *name, int name_len, struct inode_t *inode)
{
	int nb_entries_per_page, i, ret;
	struct tmpfs_dir_entry_t *de;
	struct list_head_t *pos;
	struct page_t *page;

	/* check file name */
	if (name_len <= 0 || name_len > TMPFS_NAME_LEN)
		return -EINVAL;
	
	/* compute number of entries per page */
	nb_entries_per_page = PAGE_SIZE / sizeof(struct tmpfs_dir_entry_t);
	
retry:
	/* walk through all pages */
	list_for_each(pos, &dir->u.tmp_i.i_pages) {
		page = list_entry(pos, struct page_t, list);

		/* walk through all entries */
		for (i = 0; i < nb_entries_per_page; i++) {
			de = (struct tmpfs_dir_entry_t *) (PAGE_ADDRESS(page) + i * sizeof(struct tmpfs_dir_entry_t));
			if (!de->d_inode)
				goto found;
		}
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
int tmpfs_lookup(struct inode_t *dir, const char *name, size_t name_len, struct inode_t **res_inode)
{
	struct tmpfs_dir_entry_t *de;

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
int tmpfs_create(struct inode_t *dir, const char *name, size_t name_len, mode_t mode, struct inode_t **res_inode)
{
	struct inode_t *inode, *tmp;
	int ret;

	/* check directory */
	*res_inode = NULL;
	if (!dir)
		return -ENOENT;

	/* check if file already exists */
	dir->i_ref++;
	if (tmpfs_lookup(dir, name, name_len, &tmp) == 0) {
		iput(tmp);
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
int tmpfs_link(struct inode_t *old_inode, struct inode_t *dir, const char *name, size_t name_len)
{
	struct tmpfs_dir_entry_t *de;
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
	old_inode->i_dirt = 1;

	/* release inodes */
	iput(old_inode);
	iput(dir);

	return 0;
}

/*
 * Remove a file.
 */
int tmpfs_unlink(struct inode_t *dir, const char *name, size_t name_len)
{
	struct tmpfs_dir_entry_t *de;
	struct inode_t *inode;

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
	memset(de, 0, sizeof(struct tmpfs_dir_entry_t));

	/* update directory */
	dir->i_ctime = dir->i_mtime = CURRENT_TIME;
	dir->i_dirt = 1;

	/* update inode */
	inode->i_ctime = dir->i_ctime;
	inode->i_nlinks--;
	inode->i_dirt = 1;

	/* release inode */
	iput(inode);
	iput(dir);

	return 0;
}

/*
 * Create a symbolic link.
 */
int tmpfs_symlink(struct inode_t *dir, const char *name, size_t name_len, const char *target)
{
	struct tmpfs_dir_entry_t *de;
	struct inode_t *inode;
	struct page_t *page;
	char *buf;
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
	page = list_first_entry(&inode->u.tmp_i.i_pages, struct page_t, list);
	buf = (char *) PAGE_ADDRESS(page);

	/* write file name on first page */
	for (i = 0; target[i] && i < PAGE_SIZE - 1; i++)
		buf[i] = target[i];
	buf[i] = 0;

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
int tmpfs_mkdir(struct inode_t *dir, const char *name, size_t name_len, mode_t mode)
{
	struct tmpfs_dir_entry_t *de;
	struct inode_t *inode;
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
	dir->i_dirt = 1;

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
int tmpfs_rmdir(struct inode_t *dir, const char *name, size_t name_len)
{
	struct tmpfs_dir_entry_t *de;
	struct inode_t *inode;

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
	memset(de, 0, sizeof(struct tmpfs_dir_entry_t));

	/* update dir */
	dir->i_ctime = dir->i_mtime = CURRENT_TIME;
	dir->i_nlinks--;
	dir->i_dirt = 1;

	/* update inode */
	inode->i_ctime = dir->i_ctime;
	inode->i_nlinks = 0;
	inode->i_dirt = 1;

	/* release inode and directory */
	iput(inode);
	iput(dir);

	return 0;
}

/*
 * Rename a file.
 */
int tmpfs_rename(struct inode_t *old_dir, const char *old_name, size_t old_name_len,
	         struct inode_t *new_dir, const char *new_name, size_t new_name_len)
{
	struct inode_t *old_inode = NULL, *new_inode = NULL;
	struct tmpfs_dir_entry_t *old_de, *new_de;
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
		new_inode->i_dirt = 1;
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
	old_dir->i_dirt = 1;
	new_dir->i_atime = new_dir->i_mtime = CURRENT_TIME;
	new_dir->i_dirt = 1;

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
int tmpfs_mknod(struct inode_t *dir, const char *name, size_t name_len, mode_t mode, dev_t dev)
{
	struct inode_t *inode, *tmp;
	int ret;

	/* check directory */
	if (!dir)
		return -ENOENT;

	/* check if file already exists */
	dir->i_ref++;
	if (tmpfs_lookup(dir, name, name_len, &tmp) == 0) {
		iput(tmp);
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
