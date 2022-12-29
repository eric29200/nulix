#include <fs/fs.h>
#include <fs/minix_fs.h>
#include <proc/sched.h>
#include <drivers/tty.h>
#include <stderr.h>
#include <fcntl.h>

/*
 * Test if a name matches a directory entry.
 */
static inline int minix_name_match(const char *name1, size_t len1, const char *name2, size_t max_len)
{
	/* check overflow */
	if (len1 > max_len)
		return 0;

	return strncmp(name1, name2, len1) == 0 && (len1 == max_len || name2[len1] == 0);
}

/*
 * Find an entry inside a directory.
 */
static struct buffer_head_t *minix_find_entry(struct inode_t *dir, const char *name, int name_len, void **res_de)
{
	struct minix_sb_info_t *sbi = minix_sb(dir->i_sb);
	int nb_entries, nb_entries_per_block, i;
	struct buffer_head_t *bh = NULL;
	struct minix3_dir_entry_t *de3;
	char *de, *de_name;

	/* check file name length */
	if (name_len <= 0 || name_len > sbi->s_name_len)
		return NULL;

	/* compute number of entries in directory */
	nb_entries = dir->i_size / sbi->s_dirsize;
	nb_entries_per_block = dir->i_sb->s_blocksize / sbi->s_dirsize;

	/* walk through all entries */
	for (i = 0; i < nb_entries; i++) {
		/* read next block if needed */
		if (i % nb_entries_per_block == 0) {
			/* release previous block */
			brelse(bh);

			/* get next block */
			bh = minix_getblk(dir, i / nb_entries_per_block, 0);
			if (!bh)
				return NULL;
		}

		/* get directory entry */
		de = bh->b_data + (i % nb_entries_per_block) * sbi->s_dirsize;
		de3 = (struct minix3_dir_entry_t *) de;
		de_name = de3->d_name;

		/* name match */
		if (minix_name_match(name, name_len, de_name, sbi->s_dirsize)) {
			*res_de = de;
			return bh;
		}
	}

	/* free block buffer */
	brelse(bh);
	return NULL;
}

/*
 * Add an entry to a directory.
 */
static int minix_add_entry(struct inode_t *dir, const char *name, int name_len, struct inode_t *inode)
{
	struct minix_sb_info_t *sbi = minix_sb(dir->i_sb);
	int nb_entries, nb_entries_per_block, i;
	struct minix3_dir_entry_t *de3 = NULL;
	struct buffer_head_t *bh = NULL;
	char *de, *de_name;
	ino_t de_ino;

	/* check file name */
	if (name_len <= 0 || name_len > sbi->s_name_len)
		return -EINVAL;

	/* compute number of entries in directory */
	nb_entries = dir->i_size / sbi->s_dirsize;
	nb_entries_per_block = dir->i_sb->s_blocksize / sbi->s_dirsize;

	/* walk through all entries */
	for (i = 0; i <= nb_entries; i++) {
		/* read next block if needed */
		if (i % nb_entries_per_block == 0) {
			/* release previous block */
			brelse(bh);

			/* get next block */
			bh = minix_getblk(dir, i / nb_entries_per_block, 1);
			if (!bh)
				return -EIO;
		}

		/* last entry : update directory size */
		if (i == nb_entries) {
			dir->i_size = (i + 1) * sbi->s_dirsize;
			dir->i_dirt = 1;
		}

		/* get directory entry */
		de = bh->b_data + (i % nb_entries_per_block) * sbi->s_dirsize;
		de3 = (struct minix3_dir_entry_t *) de;
		de_ino = de3->d_inode;
		de_name = de3->d_name;

		/* found a free entry */
		if (!de_ino)
			goto found_free_entry;
	}

	/* free block buffer */
	brelse(bh);
	return -EINVAL;
found_free_entry:
	/* set new entry */
	memset(de_name, 0, sbi->s_name_len);
	memcpy(de_name, name, name_len);

	/* set new entry inode */
	de3->d_inode = inode->i_ino;

	/* mark buffer dirty and release it */
	bh->b_dirt = 1;
	brelse(bh);

	/* update parent directory */
	dir->i_mtime = dir->i_ctime = CURRENT_TIME;
	dir->i_dirt = 1;

	return 0;
}

/*
 * Check if a directory is empty.
 */
static int minix_empty_dir(struct inode_t *dir)
{
	struct minix_sb_info_t *sbi = minix_sb(dir->i_sb);
	int nb_entries, nb_entries_per_block, i;
	struct buffer_head_t *bh = NULL;
	ino_t ino;
	char *de;

	/* check if dir is a directory */
	if (S_ISREG(dir->i_mode))
		return 0;

	/* compute number of entries in directory */
	nb_entries = dir->i_size / sbi->s_dirsize;
	nb_entries_per_block = dir->i_sb->s_blocksize / sbi->s_dirsize;

	/* walk through all entries */
	for (i = 0; i < nb_entries; i++) {
		/* read next block if needed */
		if (i % nb_entries_per_block == 0) {
			/* release previous block */
			brelse(bh);

			/* get next block */
			bh = minix_getblk(dir, i / nb_entries_per_block, 0);
			if (!bh)
				return 0;
		}

		/* skip first 2 entries "." and ".." */
		if (i < 2)
			continue;

		/* get inode number */
		de = bh->b_data + (i % nb_entries_per_block) * sbi->s_dirsize;
		ino = ((struct minix3_dir_entry_t *) de)->d_inode;

		/* found an entry : directory is not empty */
		if (ino) {
			brelse(bh);
			return 0;
		}
	}

	/* free block buffer */
	brelse(bh);
	return 1;
}

/*
 * Lookup for a file in a directory.
 */
int minix_lookup(struct inode_t *dir, const char *name, size_t name_len, struct inode_t **res_inode)
{
	struct buffer_head_t *bh;
	ino_t ino;
	void *de;

	/* check dir */
	if (!dir)
		return -ENOENT;

	/* dir must be a directory */
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
	ino = ((struct minix3_dir_entry_t *) de)->d_inode;

	/* release block buffer */
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
	struct inode_t *inode, *tmp;
	ino_t ino;
	int err;

	/* check directory */
	*res_inode = NULL;
	if (!dir)
		return -ENOENT;

	/* check if file already exists */
	dir->i_ref++;
	if (minix_lookup(dir, name, name_len, &tmp) == 0) {
		iput(tmp);
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
	inode->i_op = &minix_file_iops;
	inode->i_mode = S_IFREG | mode;
	inode->i_dirt = 1;

	/* add new entry to dir */
	err = minix_add_entry(dir, name, name_len, inode);
	if (err) {
		inode->i_nlinks--;
		iput(inode);
		iput(dir);
		return err;
	}

	/* release inode (to write it on disk) */
	ino = inode->i_ino;
	iput(inode);

	/* read inode from disk */
	*res_inode = iget(dir->i_sb, ino);
	if (!*res_inode) {
		iput(dir);
		return -EACCES;
	}

	/* release directory */
	iput(dir);

	return 0;
}

/*
 * Make a new name for a file = hard link.
 */
int minix_link(struct inode_t *old_inode, struct inode_t *dir, const char *name, size_t name_len)
{
	struct buffer_head_t *bh;
	void *de;
	int err;

	/* check if new file exists */
	bh = minix_find_entry(dir, name, name_len, &de);
	if (bh) {
		brelse(bh);
		iput(old_inode);
		iput(dir);
		return -EEXIST;
	}

	/* add entry */
	err = minix_add_entry(dir, name, name_len, old_inode);
	if (err) {
		iput(old_inode);
		iput(dir);
		return err;
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
int minix_unlink(struct inode_t *dir, const char *name, size_t name_len)
{
	struct minix_sb_info_t *sbi;
	struct buffer_head_t *bh;
	struct inode_t *inode;
	ino_t ino;
	void *de;

	/* get directory entry */
	bh = minix_find_entry(dir, name, name_len, &de);
	if (!bh) {
		iput(dir);
		return -ENOENT;
	}

	/* get inode number */
	sbi = minix_sb(dir->i_sb);
	ino = ((struct minix3_dir_entry_t *) de)->d_inode;

	/* get inode */
	inode = iget(dir->i_sb, ino);
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

	/* reset directory entry */
	memset(de, 0, sbi->s_dirsize);
	bh->b_dirt = 1;
	brelse(bh);

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
int minix_symlink(struct inode_t *dir, const char *name, size_t name_len, const char *target)
{
	struct buffer_head_t *bh;
	struct inode_t *inode;
	void *de;
	size_t i;
	int err;

	/* create a new inode */
	inode = minix_new_inode(dir->i_sb);
	if (!inode) {
		iput(dir);
		return -ENOSPC;
	}

	/* set new inode */
	inode->i_uid = current_task->uid;
	inode->i_gid = current_task->gid;
	inode->i_op = &minix_symlink_iops;
	inode->i_mode = S_IFLNK | (0777 & ~current_task->fs->umask);
	inode->i_dirt = 1;

	/* get/create first block */
	bh = minix_getblk(inode, 0, 1);
	if(!bh) {
		inode->i_nlinks = 0;
		iput(inode);
		iput(dir);
		return -ENOSPC;
	}

	/* write file name on first block */
	for (i = 0; target[i] && i < inode->i_sb->s_blocksize - 1; i++)
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
		brelse(bh);
		inode->i_nlinks = 0;
		iput(inode);
		iput(dir);
		return -EEXIST;
	}

	/* add entry */
	err = minix_add_entry(dir, name, name_len, inode);
	if (err) {
		iput(inode);
		iput(dir);
		return err;
	}

	/* release inode */
	iput(inode);
	iput(dir);

	return 0;
}

/*
 * Create a directory.
 */
int minix_mkdir(struct inode_t *dir, const char *name, size_t name_len, mode_t mode)
{
	struct minix3_dir_entry_t *de3;
	struct minix_sb_info_t *sbi;
	struct buffer_head_t *bh;
	struct inode_t *inode;
	void *de;
	int err;

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
		return -ENOMEM;
	}

	/* set inode */
	sbi = minix_sb(dir->i_sb);
	inode->i_uid = current_task->uid;
	inode->i_gid = current_task->gid;
	inode->i_op = &minix_dir_iops;
	inode->i_mode = S_IFDIR | (mode & ~current_task->fs->umask & 0777);
	inode->i_nlinks = 2;
	inode->i_size = sbi->s_dirsize * 2;
	inode->i_dirt = 1;

	/* get/create first block */
	bh = minix_getblk(inode, 0, 1);
	if (!bh) {
		inode->i_nlinks = 0;
		iput(inode);
		iput(dir);
		return -ENOSPC;
	}

	/* add '.' entry */
	de3 = (struct minix3_dir_entry_t *) bh->b_data;
	de3->d_inode = inode->i_ino;
	strcpy(de3->d_name, ".");

	/* add '..' entry */
	de3 = (struct minix3_dir_entry_t *) (bh->b_data + sbi->s_dirsize);
	de3->d_inode = dir->i_ino;
	strcpy(de3->d_name, "..");

	/* release first block */
	bh->b_dirt = 1;
	brelse(bh);

	/* add entry to parent dir */
	err = minix_add_entry(dir, name, name_len, inode);
	if (err) {
		inode->i_nlinks = 0;
		iput(inode);
		iput(dir);
		return err;
	}

	/* update directory links and mark it dirty */
	dir->i_nlinks++;
	dir->i_dirt = 1;

	/* release inode */
	iput(dir);
	iput(inode);

	return 0;
}

/*
 * Remove a directory.
 */
int minix_rmdir(struct inode_t *dir, const char *name, size_t name_len)
{
	struct minix_sb_info_t *sbi;
	struct buffer_head_t *bh;
	struct inode_t *inode;
	ino_t ino;
	void *de;

	/* check if file exists */
	bh = minix_find_entry(dir, name, name_len, &de);
	if (!bh) {
		iput(dir);
		return -ENOENT;
	}

	/* get inode number */
	sbi = minix_sb(dir->i_sb);
	ino = ((struct minix3_dir_entry_t *) de)->d_inode;

	/* get inode */
	inode = iget(dir->i_sb, ino);
	if (!inode) {
		brelse(bh);
		iput(dir);
		return -ENOENT;
	}

	/* remove directories only and do not allow to remove '.' */
	if (!S_ISDIR(inode->i_mode) || inode->i_ino == dir->i_ino) {
		brelse(bh);
		iput(inode);
		iput(dir);
		return -EPERM;
	}

	/* directory must be empty */
	if (!minix_empty_dir(inode)) {
		brelse(bh);
		iput(inode);
		iput(dir);
		return -EPERM;
	}

	/* reset entry */
	memset(de, 0, sbi->s_dirsize);
	bh->b_dirt = 1;
	brelse(bh);

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
int minix_rename(struct inode_t *old_dir, const char *old_name, size_t old_name_len,
		 struct inode_t *new_dir, const char *new_name, size_t new_name_len)
{
	struct inode_t *old_inode = NULL, *new_inode = NULL;
	struct buffer_head_t *old_bh = NULL, *new_bh = NULL;
	struct minix_sb_info_t *sbi;
	ino_t old_ino, new_ino;
	void *old_de, *new_de;
	int err;

	/* find old entry */
	old_bh = minix_find_entry(old_dir, old_name, old_name_len, &old_de);
	if (!old_bh) {
		err = -ENOENT;
		goto out;
	}

	/* get old inode number */
	sbi = minix_sb(old_dir->i_sb);
	old_ino = ((struct minix3_dir_entry_t *) old_de)->d_inode;

	/* get old inode */
	old_inode = iget(old_dir->i_sb, old_ino);
	if (!old_inode) {
		err = -ENOSPC;
		goto out;
	}

	/* find new entry (if exists) or add new one */
	new_bh = minix_find_entry(new_dir, new_name, new_name_len, &new_de);
	if (new_bh) {
		/* get new inode number */
		sbi = minix_sb(new_dir->i_sb);
		new_ino = ((struct minix3_dir_entry_t *) new_de)->d_inode;

		/* get new inode */
		new_inode = iget(new_dir->i_sb, new_ino);
		if (!new_inode) {
			err = -ENOSPC;
			goto out;
		}

		/* same inode : exit */
		if (old_inode->i_ino == new_inode->i_ino) {
			err = 0;
			goto out;
		}

		/* modify new directory entry inode */
		sbi = minix_sb(old_dir->i_sb);
		((struct minix3_dir_entry_t *) new_de)->d_inode = old_inode->i_ino;

		/* update new inode */
		new_inode->i_nlinks--;
		new_inode->i_atime = new_inode->i_mtime = CURRENT_TIME;
		new_inode->i_dirt = 1;
	} else {
		/* add new entry */
		err = minix_add_entry(new_dir, new_name, new_name_len, old_inode);
		if (err)
			goto out;
	}

	/* cancel old directory entry */
	sbi = minix_sb(old_dir->i_sb);
	((struct minix3_dir_entry_t *) old_de)->d_inode = 0;
	memset(((struct minix3_dir_entry_t *) old_de)->d_name, 0, sbi->s_name_len);
	old_bh->b_dirt = 1;

	/* update old and new directories */
	old_dir->i_atime = old_dir->i_mtime = CURRENT_TIME;
	old_dir->i_dirt = 1;
	new_dir->i_atime = new_dir->i_mtime = CURRENT_TIME;
	new_dir->i_dirt = 1;

	err = 0;
out:
	/* release buffers and inodes */
	brelse(old_bh);
	brelse(new_bh);
	iput(old_inode);
	iput(new_inode);
	iput(old_dir);
	iput(new_dir);

	return err;
}

/*
 * Create a node.
 */
int minix_mknod(struct inode_t *dir, const char *name, size_t name_len, mode_t mode, dev_t dev)
{
	struct inode_t *inode, *tmp;
	int err;

	/* check directory */
	if (!dir)
		return -ENOENT;

	/* check if file already exists */
	dir->i_ref++;
	if (minix_lookup(dir, name, name_len, &tmp) == 0) {
		iput(tmp);
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
	inode->i_rdev = dev;
	inode->i_dirt = 1;

	/* add new entry to dir */
	err = minix_add_entry(dir, name, name_len, inode);
	if (err) {
		inode->i_nlinks--;
		iput(inode);
		iput(dir);
		return err;
	}

	/* release inode (to write it on disk) */
	iput(inode);
	iput(dir);

	return 0;
}
