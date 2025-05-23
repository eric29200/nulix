#include <fs/fs.h>
#include <fs/minix_fs.h>
#include <string.h>
#include <fcntl.h>

/*
 * Write to a file.
 */
int minix_file_write(struct file *filp, const char *buf, int count)
{
	struct super_block *sb = filp->f_inode->i_sb;
	size_t pos, nr_chars, left;
	struct buffer_head *bh;

	/* handle append flag */
	if (filp->f_flags & O_APPEND)
		filp->f_pos = filp->f_inode->i_size;

	for (left = count; left > 0; ) {
		/* get/create block */
		bh = minix_bread(filp->f_inode, filp->f_pos / sb->s_blocksize, 1);
		if (!bh)
			break;

		/* find position and number of chars to read */
		pos = filp->f_pos % sb->s_blocksize;
		nr_chars = sb->s_blocksize - pos <= left ? sb->s_blocksize - pos : left;

		/* copy into buffer */
		memcpy(bh->b_data + pos, buf, nr_chars);

		/* release block */
		mark_buffer_dirty(bh);
		brelse(bh);

		/* update page cache */
		update_vm_cache(filp->f_inode, buf, pos, nr_chars);

		/* update sizes */
		filp->f_pos += nr_chars;
		buf += nr_chars;
		left -= nr_chars;

		/* end of file : grow it and mark inode dirty */
		if (filp->f_pos > filp->f_inode->i_size) {
			filp->f_inode->i_size = filp->f_pos;
			filp->f_inode->i_dirt = 1;
		}
	}

	filp->f_inode->i_mtime = filp->f_inode->i_ctime = CURRENT_TIME;
	filp->f_inode->i_dirt = 1;
	return count - left;
}
