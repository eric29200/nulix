#include <fs/dev_fs.h>
#include <proc/sched.h>
#include <stderr.h>
#include <fcntl.h>

/*
 * Test if a name matches a directory entry.
 */
static inline int devfs_name_match(const char *name1, size_t len1, const char *name2, size_t max_len)
{
	/* check overflow */
	if (len1 > max_len)
		return 0;

	return strncmp(name1, name2, len1) == 0 && (len1 == max_len || name2[len1] == 0);
}

/*
 * Find an entry inside a directory.
 */
static struct devfs_dir_entry_t *devfs_find_entry(struct inode_t *dir, const char *name, int name_len)
{
	int nb_entries_per_page, i;
	struct devfs_dir_entry_t *de;
	struct list_head_t *pos;
	struct page_t *page;

	/* check file name length */
	if (name_len <= 0 || name_len > DEVFS_NAME_LEN)
		return NULL;

	/* compute number of entries per page */
	nb_entries_per_page = PAGE_SIZE / sizeof(struct devfs_dir_entry_t);

	/* walk through all pages */
	list_for_each(pos, &dir->u.dev_i.i_pages) {
		page = list_entry(pos, struct page_t, list);

		/* walk through all entries */
		for (i = 0; i < nb_entries_per_page; i++) {
			de = (struct devfs_dir_entry_t *) (PAGE_ADDRESS(page) + i * sizeof(struct devfs_dir_entry_t));
			if (devfs_name_match(name, name_len, de->d_name, DEVFS_NAME_LEN))
				return de;
		}
	}

	return NULL;
}

/*
 * Add a directory entry.
 */
int devfs_add_entry(struct inode_t *dir, const char *name, int name_len, struct inode_t *inode)
{
	int nb_entries_per_page, i, ret;
	struct devfs_dir_entry_t *de;
	struct list_head_t *pos;
	struct page_t *page;

	/* check file name */
	if (name_len <= 0 || name_len > DEVFS_NAME_LEN)
		return -EINVAL;

	/* compute number of entries per page */
	nb_entries_per_page = PAGE_SIZE / sizeof(struct devfs_dir_entry_t);

retry:
	/* walk through all pages */
	list_for_each(pos, &dir->u.dev_i.i_pages) {
		page = list_entry(pos, struct page_t, list);

		/* walk through all entries */
		for (i = 0; i < nb_entries_per_page; i++) {
			de = (struct devfs_dir_entry_t *) (PAGE_ADDRESS(page) + i * sizeof(struct devfs_dir_entry_t));
			if (!de->d_inode)
				goto found;
		}
	}

	/* no free directory entry : grow directory */
	ret = devfs_inode_grow_size(dir, dir->i_size + PAGE_SIZE);
	if (ret)
		return ret;

	goto retry;
found:
	/* set new directory entry */
	de->d_inode = inode->i_ino;
	memset(de->d_name, 0, DEVFS_NAME_LEN);
	memcpy(de->d_name, name, name_len);

	/* update parent directory */
	dir->i_mtime = dir->i_ctime = CURRENT_TIME;

	return 0;
}

/*
 * Directory lookup.
 */
int devfs_lookup(struct inode_t *dir, const char *name, size_t name_len, struct inode_t **res_inode)
{
	struct devfs_dir_entry_t *de;

	/* dir must be a directory */
	if (!dir)
		return -ENOENT;
	if (!S_ISDIR(dir->i_mode)) {
		iput(dir);
		return -ENOENT;
	}

	/* find entry */
	de = devfs_find_entry(dir, name, name_len);
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
 * Remove a file.
 */
int devfs_unlink(struct inode_t *dir, const char *name, size_t name_len)
{
	struct devfs_dir_entry_t *de;
	struct inode_t *inode;

	/* get directory entry */
	de = devfs_find_entry(dir, name, name_len);
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
	memset(de, 0, sizeof(struct devfs_dir_entry_t));

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
 * Create a directory.
 */
int devfs_mkdir(struct inode_t *dir, const char *name, size_t name_len, mode_t mode)
{
	struct devfs_dir_entry_t *de;
	struct inode_t *inode;
	int ret;

	/* check if file exists */
	de = devfs_find_entry(dir, name, name_len);
	if (de) {
		iput(dir);
		return -EEXIST;
	}

	/* allocate a new inode */
	inode = devfs_new_inode(dir->i_sb, S_IFDIR | (mode & ~current_task->fs->umask & 0777), 0);
	if (!inode) {
		iput(dir);
		return -ENOMEM;
	}

	/* create "." entry in root directory */
	ret = devfs_add_entry(inode, ".", 1, inode);
	if (ret)
		goto err;

	/* create ".." entry in root directory */
	ret = devfs_add_entry(inode, "..", 2, dir);
	if (ret)
		goto err;

	/* add entry to parent dir */
	ret = devfs_add_entry(dir, name, name_len, inode);
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
 * Create a node.
 */
int devfs_mknod(struct inode_t *dir, const char *name, size_t name_len, mode_t mode, dev_t dev)
{
	struct inode_t *inode, *tmp;
	int ret;

	/* check directory */
	if (!dir)
		return -ENOENT;

	/* check if file already exists */
	dir->i_ref++;
	if (devfs_lookup(dir, name, name_len, &tmp) == 0) {
		iput(tmp);
		iput(dir);
		return -EEXIST;
	}

	/* create a new inode */
	inode = devfs_new_inode(dir->i_sb, mode, dev);
	if (!inode) {
		iput(dir);
		return -ENOSPC;
	}

	/* add new entry to dir */
	ret = devfs_add_entry(dir, name, name_len, inode);
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

/*
 * Create a symbolic link.
 */
int devfs_symlink(struct inode_t *dir, const char *name, size_t name_len, const char *target)
{
	struct devfs_dir_entry_t *de;
	struct inode_t *inode;
	struct page_t *page;
	char *buf;
	int ret, i;

	/* create a new inode */
	inode = devfs_new_inode(dir->i_sb, S_IFLNK | (0777 & ~current_task->fs->umask), 0);
	if (!inode) {
		iput(dir);
		return -ENOSPC;
	}

	/* create first page */
	ret = devfs_inode_grow_size(inode, PAGE_SIZE);
	if (ret) {
		inode->i_nlinks = 0;
		iput(inode);
		iput(dir);
		return -ENOMEM;
	}

	/* get first page */
	page = list_first_entry(&inode->u.dev_i.i_pages, struct page_t, list);
	buf = (char *) PAGE_ADDRESS(page);

	/* write file name on first page */
	for (i = 0; target[i] && i < PAGE_SIZE - 1; i++)
		buf[i] = target[i];
	buf[i] = 0;

	/* check if file exists */
	de = devfs_find_entry(dir, name, name_len);
	if (de) {
		inode->i_nlinks = 0;
		iput(inode);
		iput(dir);
		return -EEXIST;
	}

	/* add entry */
	ret = devfs_add_entry(dir, name, name_len, inode);
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