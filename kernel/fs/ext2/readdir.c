#include <fs/fs.h>
#include <fs/ext2_fs.h>

/*
 * Get directory entries.
 */
int ext2_getdents64(struct file_t *filp, void *dirp, size_t count)
{
	struct super_block_t *sb = filp->f_inode->i_sb;
	struct inode_t *inode = filp->f_inode;
	struct buffer_head_t *bh = NULL;
	struct ext2_dir_entry_t *de;
	struct dirent64_t *dirent;
	uint32_t offset, block;
	int entries_size = 0;

	/* get start offset */
	offset = filp->f_pos & (sb->s_blocksize - 1);
	dirent = (struct dirent64_t *) dirp;

	/* read block by block */
	while (filp->f_pos < inode->i_size) {
		/* read next block */
		block = filp->f_pos >> sb->s_blocksize_bits;
		bh = ext2_bread(inode, block, 0);
		if (!bh) {
			filp->f_pos += sb->s_blocksize - offset;
			continue;
		}

		/* read all entries in block */
		while (filp->f_pos < inode->i_size && offset < sb->s_blocksize) {
			/* check next entry */
			de = (struct ext2_dir_entry_t *) (bh->b_data + offset);
			if (de->d_rec_len <= 0) {
				brelse(bh);
				return entries_size;
			}

			/* skip null entry */
			if (de->d_inode == 0) {
				offset += de->d_rec_len;
				filp->f_pos += de->d_rec_len;
				continue;
			}

			/* not enough space to fill in next dir entry : break */
			if (count < sizeof(struct dirent64_t) + de->d_name_len + 1) {
				brelse(bh);
				return entries_size;
			}

			/* fill in dirent */
			dirent->d_inode = de->d_inode;
			dirent->d_off = 0;
			dirent->d_reclen = sizeof(struct dirent64_t) + de->d_name_len + 1;
			dirent->d_type = 0;
			memcpy(dirent->d_name, de->d_name, de->d_name_len);
			dirent->d_name[de->d_name_len] = 0;

			/* update offset */
			offset += de->d_rec_len;

			/* go to next entry */
			count -= dirent->d_reclen;
			entries_size += dirent->d_reclen;
			dirent = (struct dirent64_t *) ((char *) dirent + dirent->d_reclen);

			/* update file position */
			filp->f_pos += de->d_rec_len;
		}

		/* reset offset and release block buffer */
		offset = 0;
		brelse(bh);
	}

	return entries_size;
}
