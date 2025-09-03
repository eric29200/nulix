#include <fs/fs.h>
#include <fs/minix_fs.h>
#include <mm/mm.h>
#include <string.h>
#include <stderr.h>

/*
 * Get directory entries.
 */
int minix_getdents64(struct file *filp, void *dirp, size_t count)
{
	struct minix_sb_info *sbi = minix_sb(filp->f_inode->i_sb);
	struct super_block *sb = filp->f_inode->i_sb;
	struct inode *inode = filp->f_inode;
	struct buffer_head *bh = NULL;
	struct minix3_dir_entry *de;
	int entries_size = 0, ret;
	struct dirent64 *dirent;
	uint32_t offset, block;
	size_t name_len;

	/* get start offset */
	offset = filp->f_pos & (sb->s_blocksize - 1);
	dirent = (struct dirent64 *) dirp;

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
			if (de->d_inode == 0) {
				offset += sbi->s_dirsize;
				filp->f_pos += sbi->s_dirsize;
				continue;
			}

			/* fill in directory entry */
			name_len = strlen(de->d_name);
			ret = filldir(dirent, de->d_name, name_len, de->d_inode, count);
			if (ret) {
				brelse(bh);
				return entries_size;
			}

			/* update offset */
			offset += sbi->s_dirsize;

			/* go to next entry */
			count -= dirent->d_reclen;
			entries_size += dirent->d_reclen;
			dirent = (struct dirent64 *) ((char *) dirent + dirent->d_reclen);

			/* update file position */
			filp->f_pos += sbi->s_dirsize;
		}

		/* reset offset and release block buffer */
		offset = 0;
		brelse(bh);
	}

	return entries_size;
}