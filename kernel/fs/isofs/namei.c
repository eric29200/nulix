#include <fs/fs.h>
#include <fs/iso_fs.h>
#include <stderr.h>
#include <fcntl.h>

/*
 * Test if a name matches a directory entry.
 */
static inline int isofs_name_match(const char *name1, size_t len1, const char *name2, size_t len2)
{
	return len1 == len2 && memcmp(name1, name2, len1) == 0;
}

/*
 * Find an entry inside a directory.
 */
static struct buffer_head *isofs_find_entry(struct inode *dir, const char *name, size_t name_len, ino_t *res_ino, ino_t *backlink_ino)
{
	char name_tmp[ISOFS_MAX_NAME_LEN + 1], de_tmp[4096];
	uint32_t offset, block, f_pos = 0, next_offset;
	struct isofs_inode_info *isofs_inode;
	struct iso_directory_record *de;
	struct super_block *sb;
	struct buffer_head *bh;
	size_t name_len_tmp;
	int de_len;
	ino_t ino;

	/* reset result inode */
	*res_ino = 0;

	/* check directory */
	if (!dir)
		return NULL;

	/* get inode and super block */
	isofs_inode = &dir->u.iso_i;
	sb = dir->i_sb;

	/* compute position */
	offset = f_pos & (sb->s_blocksize - 1);
	block = (isofs_inode->i_first_extent >> sb->s_blocksize_bits) + (f_pos >> sb->s_blocksize_bits);
	if (!block)
		return NULL;

	/* read first block */
	bh = bread(sb->s_dev, block, sb->s_blocksize);
	if (!bh)
		return NULL;

	/* walk through directory */
	while (f_pos < dir->i_size) {
		/* read next block */
		if (offset >= sb->s_blocksize) {
			/* release previous block buffer */
			brelse(bh);

			/* compute next block */
			offset = 0;
			block = (isofs_inode->i_first_extent >> sb->s_blocksize_bits) + (f_pos >> sb->s_blocksize_bits);
			if (!block)
				return NULL;

			/* read next block */
			bh = bread(sb->s_dev, block, sb->s_blocksize);
			if (!bh)
				return NULL;
		}

		/* get directory entry and compute inode */
		de = (struct iso_directory_record *) (bh->b_data + offset);
		ino = (block << sb->s_blocksize_bits) + (offset & (sb->s_blocksize - 1));
		de_len = *((unsigned char *) de);

		/* zero entry : go to next sector */
		if (!de_len) {
			/* release previous block buffer */
			brelse(bh);

			/* update file position */
			f_pos = ((f_pos & ~(sb->s_blocksize - 1)) + sb->s_blocksize);

			/* compute next block */
			offset = 0;
			block = (isofs_inode->i_first_extent >> sb->s_blocksize_bits) + (f_pos >> sb->s_blocksize_bits);
			if (!block)
				return NULL;

			/* read next block */
			bh = bread(sb->s_dev, block, sb->s_blocksize);
			if (!bh)
				return NULL;

			continue;
		}

		/* directory entry is on 2 blocks */
		next_offset = offset + de_len;
		if (next_offset > sb->s_blocksize) {
			/* copy first fragment */
			next_offset &= (sb->s_blocksize - 1);
			memcpy(de_tmp, de, sb->s_blocksize - offset);

			/* read next block buffer */
			brelse(bh);
			block = (isofs_inode->i_first_extent >> sb->s_blocksize_bits) + ((f_pos + de_len) >> sb->s_blocksize_bits);
			bh = bread(sb->s_dev, block, sb->s_blocksize);
			if (!bh)
				return NULL;

			/* copy 2nd fragment */
			memcpy(&de_tmp[sb->s_blocksize - offset], bh->b_data, next_offset);
		}
		offset = next_offset;

		/* handle ".", ".." and normal entries */
		if (de->name_len[0] == 1 && de->name[0] == 0) {
			ino = dir->i_ino;
			*backlink_ino = 0;
			strcpy(name_tmp, ".");
			name_len_tmp = 1;
		} else if (de->name_len[0] == 1 && de->name[0] == 1) {
			ino = isofs_parent_ino(dir);
			*backlink_ino = 0;
			strcpy(name_tmp, "..");
			name_len_tmp = 2;
		} else {
			name_len_tmp = get_rock_ridge_filename(de, name_tmp, dir);
			if (!name_len_tmp)
				name_len_tmp = isofs_name_translate(de->name, de->name_len[0], name_tmp);
			*backlink_ino = dir->i_ino;
		}

		/* name match */
		if (isofs_name_match(name, name_len, name_tmp, name_len_tmp)) {
			*res_ino = ino;
			return bh;
		}

		/* go to next entry */
		f_pos += de_len;
	}

	brelse(bh);
	return NULL;
}

/*
 * Lookup for a file in a directory.
 */
int isofs_lookup(struct inode *dir, const char *name, size_t name_len, struct inode **res_inode)
{
	struct buffer_head *bh = NULL;
	ino_t ino, backlink;

	/* check dir */
	if (!dir)
		return -ENOENT;

	/* dir must be a directory */
	if (!S_ISDIR(dir->i_mode)) {
		iput(dir);
		return -ENOENT;
	}

	/* find entry */
	bh = isofs_find_entry(dir, name, name_len, &ino, &backlink);
	if (!bh) {
		iput(dir);
		return -ENOENT;
	}

	/* release block buffer */
	brelse(bh);

	/* get inode */
	*res_inode = iget(dir->i_sb, ino);
	if (!*res_inode) {
		iput(dir);
		return -EACCES;
	}

	/* save backlink */
	if (backlink)
		dir->u.iso_i.i_backlink = backlink;

	iput(dir);
	return 0;
}
