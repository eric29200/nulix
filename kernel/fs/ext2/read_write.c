#include <fs/fs.h>
#include <fs/ext2_fs.h>
#include <fcntl.h>

/*
 * Read a Ext2 file.
 */
int ext2_file_read(struct file *filp, char *buf, int count)
{
	size_t pos, nr_chars, left;
	struct buffer_head *bh;

	/* adjust size */
	if (filp->f_pos + count > filp->f_inode->i_size)
		count = filp->f_inode->i_size - filp->f_pos;

	/* no more data to read */
	if (count <= 0)
		return 0;

	/* read block by block */
	for (left = count; left > 0;) {
		/* read block */
		bh = ext2_bread(filp->f_inode, filp->f_pos / filp->f_inode->i_sb->s_blocksize, 0);
		if (!bh)
			goto out;

		/* find position and numbers of chars to read */
		pos = filp->f_pos % filp->f_inode->i_sb->s_blocksize;
		nr_chars = filp->f_inode->i_sb->s_blocksize - pos <= left ? filp->f_inode->i_sb->s_blocksize - pos : left;

		/* copy to buffer */
		memcpy(buf, bh->b_data + pos, nr_chars);

		/* release block */
		brelse(bh);

		/* update sizes */
		filp->f_pos += nr_chars;
		buf += nr_chars;
		left -= nr_chars;
	}

out:
	filp->f_inode->i_atime = CURRENT_TIME;
	filp->f_inode->i_dirt = 1;
	return count - left;
}

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
			goto out;

		/* find position and numbers of chars to read */
		pos = filp->f_pos % filp->f_inode->i_sb->s_blocksize;
		nr_chars = filp->f_inode->i_sb->s_blocksize - pos <= left ? filp->f_inode->i_sb->s_blocksize - pos : left;

		/* copy to buffer */
		memcpy(bh->b_data + pos, buf, nr_chars);

		/* release block */
		bh->b_dirt = 1;
		brelse(bh);

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

out:
	filp->f_inode->i_mtime = filp->f_inode->i_ctime = CURRENT_TIME;
	filp->f_inode->i_dirt = 1;
	return count - left;
}
