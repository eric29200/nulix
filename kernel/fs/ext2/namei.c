#include <fs/fs.h>
#include <fs/ext2_fs.h>
#include <proc/sched.h>
#include <stdio.h>
#include <stderr.h>
#include <fcntl.h>

/*
 * Test file names equality.
 */
static inline int ext2_name_match(const char *name, size_t len, struct ext2_dir_entry *de)
{
	/* check dir entry */
	if (!de || !de->d_inode || len > EXT2_NAME_LEN)
		return 0;

	return len == de->d_name_len && !memcmp(name, de->d_name, len);
}

/*
 * Find a Ext2 entry in a directory.
 */
static struct buffer_head *ext2_find_entry(struct inode *dir, const char *name, size_t name_len, struct ext2_dir_entry **res_de)
{
	struct buffer_head *bh = NULL;
	struct ext2_dir_entry *de;
	uint32_t offset, block, pos;

	/* read block by block */
	for (block = 0, offset = 0, pos = 0; pos < dir->i_size; block++) {
		/* read next block */
		bh = ext2_bread(dir, block, 0);
		if (!bh)
			continue;

		/* read all entries in block */
		while (offset < dir->i_size && offset < dir->i_sb->s_blocksize) {
			/* check next entry */
			de = (struct ext2_dir_entry *) (bh->b_data + offset);
			if (de->d_rec_len <= 0) {
				brelse(bh);
				return NULL;
			}

			/* skip null entry */
			if (de->d_inode == 0) {
				offset += de->d_rec_len;
				pos += de->d_rec_len;
				continue;
			}

			/* check name */
			if (ext2_name_match(name, name_len, de)) {
				*res_de = de;
				return bh;
			}

			/* update offset */
			offset += de->d_rec_len;
			pos += de->d_rec_len;
		}

		/* reset offset and release block buffer */
		offset = 0;
		brelse(bh);
	}

	return NULL;
}

/*
 * Add a Ext2 entry in a directory.
 */
static int ext2_add_entry(struct inode *dir, const char *name, size_t name_len, struct inode *inode)
{
	struct ext2_dir_entry *de, *de1;
	struct buffer_head *bh = NULL;
	uint16_t rec_len;
	uint32_t offset;

	/* truncate name if needed */
	if (name_len > EXT2_NAME_LEN)
		name_len = EXT2_NAME_LEN;

	/* compute record length */
	rec_len = EXT2_DIR_REC_LEN(name_len);

	/* read first block */
	bh = ext2_bread(dir, 0, 0);
	if (!bh)
		return -EIO;

	/* find a free entry */
	for (de = (struct ext2_dir_entry *) bh->b_data, offset = 0;;) {
		/* read next block */
		if ((char *) de >= bh->b_data + dir->i_sb->s_blocksize) {
			/* release previous block */
			brelse(bh);

			/* read next block */
			bh = ext2_bread(dir, offset / dir->i_sb->s_blocksize, 1);
			if (!bh)
				return -EIO;

			/* get first entry */
			de = (struct ext2_dir_entry *) bh->b_data;

			/* update directory size and create a new null entry */
			if (offset >= dir->i_size) {
				de->d_inode = 0;
				de->d_rec_len = dir->i_sb->s_blocksize;
				dir->i_size = offset + dir->i_sb->s_blocksize;
				mark_inode_dirty(dir);
			}
		}

		/* check entry */
		if (de->d_rec_len <= 0) {
			brelse(bh);
			return -ENOENT;
		}

		/* free entry with enough space */
		if ((de->d_inode == 0 && de->d_rec_len >= rec_len)
				|| (de->d_rec_len >= EXT2_DIR_REC_LEN(de->d_name_len) + rec_len)) {
			/* null entry : adjust record length */
			if (de->d_inode) {
				de1 = (struct ext2_dir_entry *) ((char *) de + EXT2_DIR_REC_LEN(de->d_name_len));
				de1->d_rec_len = de->d_rec_len - EXT2_DIR_REC_LEN(de->d_name_len);
				de->d_rec_len = EXT2_DIR_REC_LEN(de->d_name_len);
				de = de1;
			}

			goto found_entry;
		}

		/* go to next entry */
		offset += de->d_rec_len;
		de = (struct ext2_dir_entry *) ((char *) de + de->d_rec_len);
	}

	brelse(bh);
	return -EINVAL;
found_entry:
	/* set new entry */
	de->d_inode = inode->i_ino;
	de->d_name_len = name_len;
	de->d_file_type = 0;
	memcpy(de->d_name, name, name_len);

	/* mark buffer dirty and release it */
	mark_buffer_dirty(bh);
	brelse(bh);

	/* update parent directory */
	dir->i_mtime = dir->i_ctime = CURRENT_TIME;
	mark_inode_dirty(dir);

	return 0;
}

/*
 * Delete an entry in a directory (by merging it with the previous entry).
 */
static int ext2_delete_entry(struct ext2_dir_entry *dir, struct buffer_head *bh)
{
	struct ext2_dir_entry *de, *pde;
	size_t i;

	/* find dir entry */
	for (i = 0, pde = NULL, de = (struct ext2_dir_entry *) bh->b_data; i < bh->b_size;) {
		/* check entry */
		if (de->d_rec_len <= 0)
			return -EIO;

		/* entry found */
		if (de == dir) {
			/* merge with previous entry */
			if (pde)
				pde->d_rec_len = pde->d_rec_len + de->d_rec_len;

			/* reset inode */
			de->d_inode = 0;

			return 0;
		}

		/* go to next entry */
		i += de->d_rec_len;
		pde = de;
		de = (struct ext2_dir_entry *) ((char *) de + de->d_rec_len);
	}

	return -ENOENT;
}

/*
 * Check if a directory is empty.
 */
static int ext2_empty_dir(struct inode *inode)
{
	struct ext2_dir_entry *de, *de1;
	struct buffer_head *bh;
	uint32_t offset;

	/* check directory size : must contain '.' and '..' */
	if (inode->i_size < EXT2_DIR_REC_LEN(1) + EXT2_DIR_REC_LEN(2)) {
		printf("[Ext2-fs] Bad directory size %d (inode = %d)\n", inode->i_size, inode->i_ino);
		return 1;
	}

	/* read first block */
	bh = ext2_bread(inode, 0, 0);
	if (!bh) {
		printf("[Ext2-fs] Bad directory (inode = %d) : no data block\n", inode->i_ino);
		return 1;
	}

	/* get first 2 entries */
	de = (struct ext2_dir_entry *) bh->b_data;
	de1 = (struct ext2_dir_entry *) ((char *) de + de->d_rec_len);

	/* first 2 entries must be '.' and '..' */
	if (de->d_inode != inode->i_ino || !de1->d_inode || strcmp(".", de->d_name) || strcmp("..", de1->d_name)) {
		printf("[Ext2-fs] Bad directory (inode = %d) : no '.' or '..'\n", inode->i_ino);
		return 1;
	}

	/* try to find an entry */
	offset = de->d_rec_len + de1->d_rec_len;
	de = (struct ext2_dir_entry *) ((char *) de1 + de1->d_rec_len);
	while (offset < inode->i_size) {
		/* read next block */
		if ((char *) de >= bh->b_data + inode->i_sb->s_blocksize) {
			/* release previous block */
			brelse(bh);

			/* read next block */
			bh = ext2_bread(inode, offset / inode->i_sb->s_blocksize, 0);
			if (!bh) {
				printf("[Ext2-fs] Directory (inode = %d) contains a hole at offset %d\n", inode->i_ino, offset);
				offset += inode->i_sb->s_blocksize;
				continue;
			}

			/* get first entry */
			de = (struct ext2_dir_entry *) bh->b_data;
		}

		/* check entry */
		if (de->d_rec_len <= 0) {
			brelse(bh);
			return 1;
		}

		/* entry found */
		if (de->d_inode) {
			brelse(bh);
			return 0;
		}

		/* go to next entry */
		offset += de->d_rec_len;
		de = (struct ext2_dir_entry *) ((char *) de + de->d_rec_len);
	}

	/* no entry */
	brelse(bh);
	return 1;
}

/*
 * Lookup for a file in a directory.
 */
int ext2_lookup(struct inode *dir, const char *name, size_t name_len, struct inode **res_inode)
{
	struct ext2_dir_entry *de;
	struct buffer_head *bh;
	ino_t ino;

	/* check dir */
	if (!dir)
		return -ENOENT;

	/* dir must be a directory */
	if (!S_ISDIR(dir->i_mode)) {
		iput(dir);
		return -ENOENT;
	}

	/* find entry */
	bh = ext2_find_entry(dir, name, name_len, &de);
	if (!bh) {
		iput(dir);
		return -ENOENT;
	}

	/* get inode number */
	ino = de->d_inode;

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
int ext2_create(struct inode *dir, const char *name, size_t name_len, mode_t mode, struct inode **res_inode)
{
	struct ext2_dir_entry *de;
	struct buffer_head *bh;
	struct inode *inode;
	ino_t ino;
	int err;

	/* check directory */
	*res_inode = NULL;
	if (!dir)
		return -ENOENT;

	/* check if file already exists */
	dir->i_count++;
	bh = ext2_find_entry(dir, name, name_len, &de);
	if (bh) {
		brelse(bh);
		iput(dir);
		return -EEXIST;
	}

	/* create a new inode */
	inode = ext2_new_inode(dir, S_IFREG | (mode & ~current_task->fs->umask & 0777));
	if (!inode) {
		iput(dir);
		return -ENOSPC;
	}

	/* set inode */
	inode->i_op = &ext2_file_iops;
	mark_inode_dirty(inode);

	/* add new entry to dir */
	err = ext2_add_entry(dir, name, name_len, inode);
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
 * Make a Ext2 directory.
 */
int ext2_mkdir(struct inode *dir, struct dentry *dentry, mode_t mode)
{
	struct ext2_dir_entry *de;
	struct buffer_head *bh;
	struct inode *inode;
	int err;

	/* check if file exists */
	bh = ext2_find_entry(dir, dentry->d_name.name, dentry->d_name.len, &de);
	if (bh) {
		brelse(bh);
		return -EEXIST;
	}

	/* allocate a new inode */
	inode = ext2_new_inode(dir, S_IFDIR | (mode & ~current_task->fs->umask & 0777));
	if (!inode)
		return -ENOSPC;

	/* set inode */
	inode->i_op = &ext2_dir_iops;
	inode->i_nlinks = 2;
	inode->i_size = inode->i_sb->s_blocksize;
	mark_inode_dirty(inode);

	/* read first block */
	bh = ext2_bread(inode, 0, 1);
	if (!bh) {
		inode->i_nlinks = 0;
		iput(inode);
		return -ENOSPC;
	}

	/* add '.' entry */
	de = (struct ext2_dir_entry *) bh->b_data;
	de->d_inode = inode->i_ino;
	de->d_name_len = 1;
	de->d_rec_len = EXT2_DIR_REC_LEN(de->d_name_len);
	strcpy(de->d_name, ".");

	/* add '.' entry */
	de = (struct ext2_dir_entry *) ((char *) de + de->d_rec_len);
	de->d_inode = dir->i_ino;
	de->d_name_len = 2;
	de->d_rec_len = inode->i_sb->s_blocksize - EXT2_DIR_REC_LEN(1);
	strcpy(de->d_name, "..");

	/* release first block */
	mark_buffer_dirty(bh);
	brelse(bh);

	/* add entry to parent dir */
	err = ext2_add_entry(dir, dentry->d_name.name, dentry->d_name.len, inode);
	if (err) {
		inode->i_nlinks = 0;
		iput(inode);
		return err;
	}

	/* update directory links and mark it dirty */
	dir->i_nlinks++;
	mark_inode_dirty(dir);

	/* instantiate dentry */
	d_instantiate(dentry, inode);

	return 0;
}

/*
 * Remove a directory.
 */
int ext2_rmdir(struct inode *dir, const char *name, size_t name_len)
{
	struct ext2_dir_entry *de;
	struct buffer_head *bh;
	struct inode *inode;
	ino_t ino;
	int err;

	/* check if file exists */
	bh = ext2_find_entry(dir, name, name_len, &de);
	if (!bh) {
		iput(dir);
		return -ENOENT;
	}

	/* get inode number */
	ino = de->d_inode;

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
	if (!ext2_empty_dir(inode)) {
		brelse(bh);
		iput(inode);
		iput(dir);
		return -EPERM;
	}

	/* remove entry */
	err = ext2_delete_entry(de, bh);
	if (err)
		goto out;

	/* mark buffer diry */
	mark_buffer_dirty(bh);

	/* update dir */
	dir->i_ctime = dir->i_mtime = CURRENT_TIME;
	dir->i_nlinks--;
	mark_inode_dirty(dir);

	/* update inode */
	inode->i_ctime = dir->i_ctime;
	inode->i_nlinks = 0;
	mark_inode_dirty(inode);

out:
	brelse(bh);
	iput(inode);
	iput(dir);

	return 0;
}

/*
 * Make a new name for a file (= hard link).
 */
int ext2_link(struct inode *old_inode, struct inode *dir, const char *name, size_t name_len)
{
	struct ext2_dir_entry *de;
	struct buffer_head *bh;
	int err;

	/* check if new file exists */
	bh = ext2_find_entry(dir, name, name_len, &de);
	if (bh) {
		brelse(bh);
		iput(old_inode);
		iput(dir);
		return -EEXIST;
	}

	/* add entry */
	err = ext2_add_entry(dir, name, name_len, old_inode);
	if (err) {
		iput(old_inode);
		iput(dir);
		return err;
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
 * Unlink (remove) a file.
 */
int ext2_unlink(struct inode *dir, const char *name, size_t name_len)
{
	struct ext2_dir_entry *de;
	struct buffer_head *bh;
	struct inode *inode;
	ino_t ino, err = 0;

	/* get directory entry */
	bh = ext2_find_entry(dir, name, name_len, &de);
	if (!bh) {
		iput(dir);
		return -ENOENT;
	}

	/* get inode number */
	ino = de->d_inode;

	/* get inode */
	inode = iget(dir->i_sb, ino);
	if (!inode) {
		iput(dir);
		brelse(bh);
		return -ENOENT;
	}

	/* remove regular files only */
	if (S_ISDIR(inode->i_mode)) {
		err = -EPERM;
		goto out;
	}

	/* delete entry */
	err = ext2_delete_entry(de, bh);
	if (err)
		goto out;

	/* mark buffer dirty */
	mark_buffer_dirty(bh);

	/* update directory */
	dir->i_ctime = dir->i_mtime = CURRENT_TIME;
	mark_inode_dirty(dir);

	/* update inode */
	inode->i_ctime = dir->i_ctime;
	inode->i_nlinks--;
	mark_inode_dirty(inode);

out:
	brelse(bh);
	iput(inode);
	iput(dir);
	return err;
}

/*
 * Write a symbolic link on first data block.
 */
static int ext2_block_symlink(struct inode *inode, const char *target)
{
	struct buffer_head *bh;
	size_t i;

	/* read create first block */
	bh = ext2_bread(inode, 0, 1);
	if (!bh)
		return -EIO;

	/* write file name on first block */
	for (i = 0; target[i] && i < inode->i_sb->s_blocksize - 1; i++)
		bh->b_data[i] = target[i];
	bh->b_data[i] = 0;

	/* mark inode dirty */
	inode->i_size = i;
	mark_inode_dirty(inode);

	/* release block buffer */
	brelse(bh);

	return 0;
}

/*
 * Create a symbolic link.
 */
int ext2_symlink(struct inode *dir, const char *name, size_t name_len, const char *target)
{
	struct ext2_dir_entry *de;
	struct buffer_head *bh;
	struct inode *inode;
	size_t target_len;
	int err;

	/* check name length */
	target_len = strlen(target) + 1;
	if (target_len > dir->i_sb->s_blocksize)
		return -ENAMETOOLONG;

	/* create a new inode */
	inode = ext2_new_inode(dir, S_IFLNK | (0777 & ~current_task->fs->umask));
	if (!inode) {
		iput(dir);
		return -ENOSPC;
	}

	/* write target link */
	if (target_len > sizeof(inode->u.ext2_i.i_data)) {
		inode->i_op = &ext2_page_symlink_iops;
		err = ext2_block_symlink(inode, target);
		if (err)
			goto err;
	} else {
		inode->i_op = &ext2_fast_symlink_iops;
		memcpy((char *) &inode->u.ext2_i.i_data, target, target_len);
		inode->i_size = target_len - 1;
	}

	/* mark inode dirty */
	mark_inode_dirty(inode);

	/* check if file exists */
	bh = ext2_find_entry(dir, name, name_len, &de);
	if (bh) {
		err = -EEXIST;
		brelse(bh);
		goto err;
	}

	/* add entry */
	err = ext2_add_entry(dir, name, name_len, inode);
	if (err)
		goto err;

	/* release inode */
	iput(inode);
	iput(dir);

	return 0;
err:
	inode->i_nlinks = 0;
	iput(inode);
	iput(dir);
	return err;
}

/*
 * Rename a file.
 */
int ext2_rename(struct inode *old_dir, const char *old_name, size_t old_name_len,
		struct inode *new_dir, const char *new_name, size_t new_name_len)
{
	struct inode *old_inode = NULL, *new_inode = NULL;
	struct buffer_head *old_bh = NULL, *new_bh = NULL;
	struct ext2_dir_entry *old_de, *new_de;
	ino_t old_ino, new_ino;
	int err;

	/* find old entry */
	old_bh = ext2_find_entry(old_dir, old_name, old_name_len, &old_de);
	if (!old_bh) {
		err = -ENOENT;
		goto out;
	}

	/* get old inode number */
	old_ino = old_de->d_inode;

	/* get old inode */
	old_inode = iget(old_dir->i_sb, old_ino);
	if (!old_inode) {
		err = -ENOSPC;
		goto out;
	}

	/* find new entry (if exists) or add new one */
	new_bh = ext2_find_entry(new_dir, new_name, new_name_len, &new_de);
	if (new_bh) {
		/* get new inode number */
		new_ino = new_de->d_inode;

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
		new_de->d_inode = old_inode->i_ino;

		/* update new inode */
		new_inode->i_nlinks--;
		new_inode->i_atime = new_inode->i_mtime = CURRENT_TIME;
		mark_inode_dirty(new_inode);
	} else {
		/* add new entry */
		err = ext2_add_entry(new_dir, new_name, new_name_len, old_inode);
		if (err)
			goto out;
	}

	/* remove old directory entry */
	err = ext2_delete_entry(old_de, old_bh);
	if (err)
		goto out;

	/* mark old directory buffer dirty */
	mark_buffer_dirty(old_bh);

	/* update old and new directories */
	old_dir->i_atime = old_dir->i_mtime = CURRENT_TIME;
	mark_inode_dirty(old_dir);
	new_dir->i_atime = new_dir->i_mtime = CURRENT_TIME;
	mark_inode_dirty(new_dir);

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
int ext2_mknod(struct inode *dir, const char *name, size_t name_len, mode_t mode, dev_t dev)
{
	struct ext2_dir_entry *de;
	struct buffer_head *bh;
	struct inode *inode;
	int err;

	/* check directory */
	if (!dir)
		return -ENOENT;

	/* check if file already exists */
	dir->i_count++;
	bh = ext2_find_entry(dir, name, name_len, &de);
	if (bh) {
		brelse(bh);
		iput(dir);
		return -EEXIST;
	}

	/* create a new inode */
	inode = ext2_new_inode(dir, mode);
	if (!inode) {
		iput(dir);
		return -ENOSPC;
	}

	/* set inode */
	inode->i_rdev = dev;
	mark_inode_dirty(inode);

	/* add new entry to dir */
	err = ext2_add_entry(dir, name, name_len, inode);
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
