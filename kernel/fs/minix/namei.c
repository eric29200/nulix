#include <fs/fs.h>
#include <fs/minix_fs.h>
#include <proc/sched.h>
#include <drivers/tty.h>
#include <stderr.h>
#include <fcntl.h>

/*
 * Test if a name matches a directory entry.
 */
static inline int minix_match(const char *name, size_t len, struct minix_dir_entry_t *de)
{
	return strncmp(name, de->name, len) == 0 && (len == MINIX_FILENAME_LEN || de->name[len] == 0);
}

/*
 * Find an entry inside a directory.
 */
static struct buffer_head_t *minix_find_entry(struct inode_t *dir, const char *name, size_t name_len, struct minix_dir_entry_t **res_dir)
{
	struct minix_dir_entry_t *entries;
	struct buffer_head_t *bh = NULL;
	int nb_entries, i;

	/* check name length */
	if (!name_len || name_len > MINIX_FILENAME_LEN)
		return NULL;

	/* get number of entries */
	nb_entries = dir->i_size / sizeof(struct minix_dir_entry_t);

	/* walk through all entries */
	for (i = 0; i < nb_entries; i++) {
		/* read next block if needed */
		if (i % MINIX_DIR_ENTRIES_PER_BLOCK == 0) {
			brelse(bh);
			bh = minix_bread(dir, i / MINIX_DIR_ENTRIES_PER_BLOCK, 0);
			if (!bh)
				return NULL;

			/* get dir entries */
			entries = (struct minix_dir_entry_t *) bh->b_data;
		}

		/* check name */
		if (minix_match(name, name_len, &entries[i % MINIX_DIR_ENTRIES_PER_BLOCK])) {
			*res_dir = &entries[i % MINIX_DIR_ENTRIES_PER_BLOCK];
			return bh;
		}
	}

	/* free buffer */
	brelse(bh);
	return NULL;
}

/*
 * Add an entry to a directory.
 */
static struct buffer_head_t *minix_add_entry(struct inode_t *dir, const char *name, size_t name_len, struct minix_dir_entry_t **res_dir)
{
	struct minix_dir_entry_t *de;
	struct buffer_head_t *bh;
	int i;

	/* check file name length */
	if (!name_len || name_len > MINIX_FILENAME_LEN)
		return NULL;

	/* read first dir block */
	bh = minix_bread(dir, 0, 1);
	if (!bh)
		return NULL;

	/* try to find first free entry */
	i = 0;
	de = (struct minix_dir_entry_t *) bh->b_data;
	while (1) {
		/* read next block */
		if ((char *) de >= bh->b_data + MINIX_BLOCK_SIZE) {
			/* release previous block */
			brelse(bh);

			/* read next block */
			bh = minix_bread(dir, i / MINIX_DIR_ENTRIES_PER_BLOCK, 1);
			if (!bh) {
				i += MINIX_DIR_ENTRIES_PER_BLOCK;
				continue;
			}

			/* update entries */
			de = (struct minix_dir_entry_t *) bh->b_data;
		}

		/* last entry : create a new one */
		if (i * sizeof(struct minix_dir_entry_t) >= dir->i_size) {
			de->inode = 0;
			dir->i_size = (i + 1) * sizeof(struct minix_dir_entry_t);
			dir->i_dirt = 1;
		}

		/* free entry : exit */
		if (!de->inode) {
			bh->b_dirt = 1;
			memset(de->name, 0, MINIX_FILENAME_LEN);
			memcpy(de->name, name, name_len);
			*res_dir = de;
			return bh;
		}

		i++;
		de++;
	}
}

/*
 * Check if a directory is empty.
 */
static int minix_empty_dir(struct inode_t *inode)
{
	struct minix_dir_entry_t *de;
	struct buffer_head_t *bh;
	int len, i;

	/* check if inode is a directory */
	if (!inode || S_ISREG(inode->i_mode))
		return 0;

	/* get directory length */
	len = inode->i_size / sizeof(struct minix_dir_entry_t);
	if (len < 2 || !inode->u.minix_i.i_zone[0])
		return 0;

	/* get first zone */
	bh = minix_bread(inode, 0, 0);
	if (!bh)
		return 0;

	/* skip 2 first entries (. and ..) */
	de = (struct minix_dir_entry_t *) bh->b_data;
	de += 2;
	i = 2;

	/* go through each entry */
	while (i < len) {
		/* read next block */
		if ((void *) de >= (void *) (bh->b_data + MINIX_BLOCK_SIZE)) {
			/* release previous block buffer */
			brelse(bh);

			/* get next block buffer */
			bh = minix_bread(inode, i / MINIX_DIR_ENTRIES_PER_BLOCK, 0);
			if (!bh)
				return 0;

			/* update entry */
			de = (struct minix_dir_entry_t *) bh->b_data;
		}

		/* found an entry : directory is not empty */
		if (de->inode) {
			brelse(bh);
			return 0;
		}

		/* go to next dir entry */
		de++;
		i++;
	}

	/* no entries found : directory is empty */
	brelse(bh);
	return 1;
}

/*
 * Lookup for a file in a directory.
 */
int minix_lookup(struct inode_t *dir, const char *name, size_t name_len, struct inode_t **res_inode)
{
	struct minix_dir_entry_t *de;
	struct buffer_head_t *bh;
	ino_t ino;

	/* dir must be a directory */
	if (!dir)
		return -ENOENT;
	if (!S_ISDIR(dir->i_mode)) {
		iput(dir);
		return -ENOENT;
	}

	/* find entry */
	bh = minix_find_entry(dir, name, name_len, &de);
	if (!bh) {
		iput(dir);
		return -ENOENT;
	}

	/* get inode number */
	ino = de->inode;
	brelse(bh);

	/* get inode */
	*res_inode = iget(dir->i_sb, ino);
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
int minix_create(struct inode_t *dir, const char *name, size_t name_len, mode_t mode, struct inode_t **res_inode)
{
	struct minix_dir_entry_t *de;
	struct super_block_t *sb;
	struct buffer_head_t *bh;
	struct inode_t *inode;

	/* check directory */
	*res_inode = NULL;
	if (!dir)
		return -ENOENT;

	/* create a new inode */
	inode = minix_new_inode(dir->i_sb);
	if (!inode) {
		iput(dir);
		return -ENOSPC;
	}

	/* set inode */
	inode->i_op = &minix_file_iops;
	inode->i_mode = mode;
	inode->i_dirt = 1;

	/* add new entry to dir */
	bh = minix_add_entry(dir, name, name_len, &de);
	if (!bh) {
		inode->i_nlinks--;
		iput(inode);
		iput(dir);
		return -ENOSPC;
	}

	/* set inode entry */
	de->inode = inode->i_ino;
	sb = dir->i_sb;

	/* release current dir inode/block and new inode (to write them on disk) */
	iput(dir);
	iput(inode);

	/* read inode from disk */
	*res_inode = iget(sb, de->inode);
	if (!*res_inode) {
		brelse(bh);
		return -EACCES;
	}

	brelse(bh);
	return 0;
}

/*
 * Make a new name for a file = hard link.
 */
int minix_link(struct inode_t *old_inode, struct inode_t *dir, const char *name, size_t name_len)
{
	struct minix_dir_entry_t *de;
	struct buffer_head_t *bh;

	/* check if new file already exist */
	bh = minix_find_entry(dir, name, name_len, &de);
	if (bh) {
		brelse(bh);
		iput(dir);
		iput(old_inode);
		return -EEXIST;
	}

	/* add entry */
	bh = minix_add_entry(dir, name, name_len, &de);
	if (!bh) {
		iput(dir);
		iput(old_inode);
		return -ENOSPC;
	}

	/* update directory entry */
	de->inode = old_inode->i_ino;
	bh->b_dirt = 1;
	brelse(bh);

	/* update old inode */
	old_inode->i_nlinks++;
	old_inode->i_dirt = 1;
	iput(old_inode);

	return 0;
}

/*
 * Remove a file.
 */
int minix_unlink(struct inode_t *dir, const char *name, size_t name_len)
{
	struct minix_dir_entry_t *de;
	struct buffer_head_t *bh;
	struct inode_t *inode;

	/* check if file exists */
	bh = minix_find_entry(dir, name, name_len, &de);
	if (!bh) {
		iput(dir);
		return -ENOENT;
	}

	/* get inode */
	inode = iget(dir->i_sb, de->inode);
	if (!inode) {
		iput(dir);
		brelse(bh);
		return -ENOENT;
	}

	/* remove regular files only */
	if (S_ISDIR(inode->i_mode)) {
		iput(inode);
		iput(dir);
		brelse(bh);
		return -EPERM;
	}

	/* reset entry */
	memset(de, 0, sizeof(struct minix_dir_entry_t));
	bh->b_dirt = 1;
	brelse(bh);

	/* update inode */
	inode->i_nlinks--;
	inode->i_dirt = 1;
	iput(inode);
	iput(dir);
	return 0;
}

/*
 * Create a symbolic link.
 */
int minix_symlink(struct inode_t *dir, const char *name, size_t name_len, const char *target)
{
	struct minix_dir_entry_t *de;
	struct buffer_head_t *bh;
	struct inode_t *inode;
	int i;

	/* create a new inode */
	inode = minix_new_inode(dir->i_sb);
	if (!inode) {
		iput(dir);
		return -ENOSPC;
	}

	/* set new inode */
	inode->i_op = &minix_file_iops;
	inode->i_mode = S_IFLNK | (0777 & ~current_task->umask);
	inode->i_dirt = 1;

	/* create first block */
	inode->u.minix_i.i_zone[0] = minix_new_block(inode->i_sb);
	if (!inode->u.minix_i.i_zone[0]) {
		iput(dir);
		inode->i_nlinks--;
		iput(inode);
		return -ENOSPC;
	}

	/* read first block */
	bh = minix_bread(inode, 0, 0);
	if (!bh) {
		iput(dir);
		inode->i_nlinks--;
		iput(inode);
		return -ENOSPC;
	}

	/* write file name on first block */
	for (i = 0; target[i] && i < MINIX_BLOCK_SIZE - 1; i++)
		bh->b_data[i] = target[i];
	bh->b_data[i] = 0;
	bh->b_dirt = 1;
	brelse(bh);

	/* update inode size */
	inode->i_size = i;
	inode->i_dirt = 1;

	/* check if file exists */
	bh = minix_find_entry(dir, name, name_len, &de);
	if (bh) {
		inode->i_nlinks--;
		iput(inode);
		brelse(bh);
		iput(dir);
		return -EEXIST;
	}

	/* add entry */
	bh = minix_add_entry(dir, name, name_len, &de);
	if (!bh) {
		inode->i_nlinks--;
		iput(inode);
		iput(dir);
		return -ENOSPC;
	}

	/* set inode entry */
	de->inode = inode->i_ino;
	bh->b_dirt = 1;

	/* release inode */
	brelse(bh);
	iput(dir);
	iput(inode);

	return 0;
}

/*
 * Create a directory.
 */
int minix_mkdir(struct inode_t *dir, const char *name, size_t name_len, mode_t mode)
{
	struct minix_dir_entry_t *de;
	struct buffer_head_t *bh;
	struct inode_t *inode;

	/* check if file exists */
	bh = minix_find_entry(dir, name, name_len, &de);
	if (bh) {
		brelse(bh);
		iput(dir);
		return -EEXIST;
	}

	/* allocate a new inode */
	inode = minix_new_inode(dir->i_sb);
	if (!inode) {
		iput(dir);
		return -ENOSPC;
	}

	/* set inode */
	inode->i_size = sizeof(struct minix_dir_entry_t) * 2;
	inode->i_nlinks = 2;
	inode->i_time = CURRENT_TIME;
	inode->i_dirt = 1;
	inode->i_mode = S_IFDIR | (mode & ~current_task->umask & 0777);
	inode->i_uid = current_task->uid;
	inode->i_gid = current_task->gid;
	inode->i_op = &minix_dir_iops;

	/* read first block */
	bh = minix_bread(inode, 0, 1);
	if (!bh) {
		inode->i_nlinks = 0;
		iput(dir);
		iput(inode);
		return -ENOSPC;
	}

	/* add entry '.' */
	de = (struct minix_dir_entry_t *) bh->b_data;
	de->inode = inode->i_ino;
	strcpy(de->name, ".");

	/* add entry '..' */
	de++;
	de->inode = dir->i_ino;
	strcpy(de->name, "..");

	/* release first block */
	bh->b_dirt = 1;
	brelse(bh);

	/* add entry to parent dir */
	bh = minix_add_entry(dir, name, name_len, &de);
	if (!bh) {
		inode->i_nlinks = 0;
		iput(dir);
		iput(inode);
		return -ENOSPC;
	}

	/* set inode entry */
	de->inode = inode->i_ino;

	/* update directory links and mark directory dirty */
	dir->i_nlinks++;
	dir->i_dirt = 1;

	/* release inode */
	iput(dir);
	iput(inode);
	brelse(bh);

	return 0;
}

/*
 * Remove a directory.
 */
int minix_rmdir(struct inode_t *dir, const char *name, size_t name_len)
{
	struct minix_dir_entry_t *de;
	struct buffer_head_t *bh;
	struct inode_t *inode;

	/* check if file exists */
	bh = minix_find_entry(dir, name, name_len, &de);
	if (!bh) {
		iput(dir);
		return -ENOENT;
	}

	/* get inode */
	inode = iget(dir->i_sb, de->inode);
	if (!inode) {
		iput(dir);
		brelse(bh);
		return -ENOENT;
	}

	/* remove directories only (and avoid to remove ".") */
	if (inode == dir || !S_ISDIR(inode->i_mode)) {
		iput(inode);
		iput(dir);
		brelse(bh);
		return -EPERM;
	}

	/* directory must be empty */
	if (!minix_empty_dir(inode)) {
		iput(inode);
		iput(dir);
		brelse(bh);
		return -EPERM;
	}

	/* reset entry */
	memset(de, 0, sizeof(struct minix_dir_entry_t));
	bh->b_dirt = 1;
	brelse(bh);

	/* update dir */
	dir->i_nlinks--;
	dir->i_dirt = 1;

	/* update inode */
	inode->i_nlinks = 0;
	inode->i_dirt = 1;

	/* release dir and inode */
	iput(inode);
	iput(dir);
	return 0;
}

/*
 * Rename a file.
 */
int minix_rename(struct inode_t *old_dir, const char *old_name, size_t old_name_len,
		 struct inode_t *new_dir, const char *new_name, size_t new_name_len)
{
	struct minix_dir_entry_t *old_de = NULL, *new_de = NULL;
	struct buffer_head_t *old_bh = NULL, *new_bh = NULL;
	struct inode_t *old_inode = NULL, *new_inode = NULL;
	int ret = -ENOENT;

	/* find old entry */
	old_bh = minix_find_entry(old_dir, old_name, old_name_len, &old_de);
	if (!old_bh)
		goto out;

	/* get old inode */
	old_inode = iget(old_dir->i_sb, old_de->inode);
	if (!old_inode)
		goto out;

	/* find new entry */
	new_bh = minix_find_entry(new_dir, new_name, new_name_len, &new_de);
	if (!old_bh)
		goto out;

	/* get new inode */
	new_inode = iget(new_dir->i_sb, new_de->inode);
	if (!new_inode)
		goto out;

	/* same inode : exit */
	if (old_inode == new_inode) {
		ret = 0;
		goto out;
	}

	/* add a new entry if needed */
	if (!new_bh) {
		new_bh = minix_add_entry(new_dir, new_name, new_name_len, &new_de);
		if (!new_bh) {
			ret = -ENOSPC;
			goto out;
		}
	}

	/* cancel old directory entry */
	old_de->inode = 0;
	memset(old_de, 0, sizeof(struct minix_dir_entry_t));

	/* set new directory entry inode */
	new_de->inode = old_inode->i_ino;

	/* update new inode */
	new_inode->i_nlinks--;
	new_inode->i_time = CURRENT_TIME;
	new_inode->i_dirt = 1;

	/* update old and new directories */
	old_dir->i_time = CURRENT_TIME;
	old_dir->i_dirt = 1;
	new_dir->i_time = CURRENT_TIME;
	new_dir->i_dirt = 1;

	/* mark buffers dirty */
	old_bh->b_dirt = 1;
	new_bh->b_dirt = 1;

	ret = 0;
out:
	brelse(old_bh);
	brelse(new_bh);
	iput(old_inode);
	iput(new_inode);
	iput(old_dir);
	iput(new_dir);
	return ret;
}

/*
 * Create a node.
 */
int minix_mknod(struct inode_t *dir, const char *name, size_t name_len, mode_t mode, dev_t dev)
{
	struct minix_dir_entry_t *de;
	struct buffer_head_t *bh;
	struct inode_t *inode;

	/* no directory */
	if (!dir)
		return -ENOENT;

	/* check if entry exists */
	bh = minix_find_entry(dir, name, name_len, &de);
	if (bh) {
		brelse(bh);
		iput(dir);
		return -EEXIST;
	}

	/* create a new inode */
	inode = minix_new_inode(dir->i_sb);
	if (!inode) {
		iput(dir);
		return -ENOSPC;
	}

	/* set inode */
	inode->i_uid = current_task->uid;
	inode->i_gid = current_task->gid;
	inode->i_mode = mode;
	inode->i_time = CURRENT_TIME;
	inode->i_cdev = dev;
	inode->i_dirt = 1;

	/* set inode operations */
	if (S_ISREG(inode->i_mode))
		inode->i_op = &minix_file_iops;
	else if (S_ISDIR(inode->i_mode))
		inode->i_op = &minix_dir_iops;
	else if (S_ISCHR(inode->i_mode))
		inode->i_op = char_get_driver(inode);

	/* add inode to directory */
	bh = minix_add_entry(dir, name, name_len, &de);
	if (!bh) {
		inode->i_nlinks--;
		inode->i_dirt = 1;
		iput(inode);
		iput(dir);
		return -ENOSPC;
	}

	/* set directory entry */
	de->inode = inode->i_ino;
	bh->b_dirt = 1;

	/* release new inode and directory */
	brelse(bh);
	iput(inode);
	iput(dir);

	return 0;
}
