#include <fs/fs.h>
#include <fs/ext2_fs.h>

/*
 * Get directory entries.
 */
int ext2_getdents64(struct file *filp, void *dirp, size_t count)
{
	struct super_block *sb = filp->f_inode->i_sb;
	struct inode *inode = filp->f_inode;
	struct buffer_head *bh = NULL;
	struct ext2_dir_entry *de;
	struct dirent64 *dirent;
	int entries_size = 0, ret;
	uint32_t offset, block;

	/* get start offset */
	offset = filp->f_pos & (sb->s_blocksize - 1);
	dirent = (struct dirent64 *) dirp;

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
			de = (struct ext2_dir_entry *) (bh->b_data + offset);
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

			/* fill in directory entry */
			ret = filldir(dirent, de->d_name, de->d_name_len, de->d_inode, count);
			if (ret) {
				brelse(bh);
				return entries_size;
			}

			/* update offset */
			offset += de->d_rec_len;

			/* go to next entry */
			count -= dirent->d_reclen;
			entries_size += dirent->d_reclen;
			dirent = (struct dirent64 *) ((char *) dirent + dirent->d_reclen);

			/* update file position */
			filp->f_pos += de->d_rec_len;
		}

		/* reset offset and release block buffer */
		offset = 0;
		brelse(bh);
	}

	return entries_size;
}
