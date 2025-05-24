#include <fs/fs.h>
#include <fs/ext2_fs.h>
#include <fcntl.h>

/*
 * Write to a Ext2 file.
 */
int ext2_file_write(struct file *filp, const char *buf, int count)
{
	size_t pos, nr_chars, left;
	struct buffer_head *bh;

	/* handle append flag */
	if (filp->f_flags & O_APPEND)
		filp->f_pos = filp->f_inode->i_size;

	/* write block by block */
	for (left = count; left > 0;) {
		/* read block */
		bh = ext2_bread(filp->f_inode, filp->f_pos / filp->f_inode->i_sb->s_blocksize, 1);
		if (!bh)
			break;

		/* find position and numbers of chars to read */
		pos = filp->f_pos % filp->f_inode->i_sb->s_blocksize;
		nr_chars = filp->f_inode->i_sb->s_blocksize - pos <= left ? filp->f_inode->i_sb->s_blocksize - pos : left;

		/* copy to buffer */
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
