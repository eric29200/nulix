#include <fs/fs.h>
#include <fs/minix_fs.h>
#include <mm/mm.h>
#include <string.h>
#include <stderr.h>

/*
 * Read directory.
 */
int minix_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	struct minix_sb_info *sbi = minix_sb(filp->f_dentry->d_inode->i_sb);
	struct super_block *sb = filp->f_dentry->d_inode->i_sb;
	struct inode *inode = filp->f_dentry->d_inode;
	struct buffer_head *bh = NULL;
	struct minix3_dir_entry *de;
	uint32_t offset, block;
	size_t name_len;

	/* get start offset */
	offset = filp->f_pos & (sb->s_blocksize - 1);

	/* read block by block */
	while (filp->f_pos < inode->i_size) {
		/* read next block */
		block = filp->f_pos >> sb->s_blocksize_bits;
		bh = minix_bread(inode, block, 0);
		if (!bh) {
			filp->f_pos += sb->s_blocksize - offset;
			continue;
		}

		/* read all entries in block */
		while (filp->f_pos < inode->i_size && offset < sb->s_blocksize) {
			/* check next entry */
			de = (struct minix3_dir_entry *) (bh->b_data + offset);

			/* skip null entry */
			if (de->d_inode == 0)
				goto next;

			/* fill in directory entry */
			name_len = strlen(de->d_name);
			if (filldir(dirent, de->d_name, name_len, filp->f_pos, de->d_inode)) {
				brelse(bh);
				return 0;
			}

next:
			offset += sbi->s_dirsize;
			filp->f_pos += sbi->s_dirsize;
		}

		/* reset offset and release block buffer */
		offset = 0;
		brelse(bh);
	}

	return 0;
}