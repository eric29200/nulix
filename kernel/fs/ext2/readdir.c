#include <fs/fs.h>
#include <fs/ext2_fs.h>
#include <stdio.h>

/*
 * Read directory.
 */
int ext2_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	struct super_block *sb = filp->f_dentry->d_inode->i_sb;
	struct inode *inode = filp->f_dentry->d_inode;
	struct buffer_head *bh = NULL;
	struct ext2_dir_entry *de;
	uint32_t offset, block;

	/* get start offset */
	offset = filp->f_pos & (sb->s_blocksize - 1);

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
				return 0;
			}

			/* skip null entry */
			if (de->d_inode == 0)
				goto next;

			/* fill in directory entry */
			if (filldir(dirent, de->d_name, de->d_name_len, filp->f_pos, de->d_inode)) {
				brelse(bh);
				return 0;
			}

next:
			offset += de->d_rec_len;
			filp->f_pos += de->d_rec_len;
		}

		/* reset offset and release block buffer */
		offset = 0;
		brelse(bh);
	}

	return 0;
}