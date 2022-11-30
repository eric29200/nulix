#include <fs/fs.h>
#include <fs/minix_fs.h>
#include <string.h>
#include <fcntl.h>

/*
 * Read a file.
 */
int minix_file_read(struct file_t *filp, char *buf, int count)
{
	struct super_block_t *sb = filp->f_inode->i_sb;
	size_t pos, nb_chars, left;
	struct buffer_head_t *bh;

	/* adjust size */
	if (filp->f_pos + count > filp->f_inode->i_size)
		count = filp->f_inode->i_size - filp->f_pos;

	if (count <= 0)
		return 0;

	left = count;
	while (left > 0) {
		/* read block */
		bh = minix_bread(filp->f_inode, filp->f_pos / sb->s_blocksize, 0);
		if (!bh)
			goto out;

		/* find position and number of chars to read */
		pos = filp->f_pos % sb->s_blocksize;
		nb_chars = sb->s_blocksize - pos <= left ? sb->s_blocksize - pos : left;

		/* copy into buffer */
		memcpy(buf, bh->b_data + pos, nb_chars);

		/* release block */
		brelse(bh);

		/* update sizes */
		filp->f_pos += nb_chars;
		buf += nb_chars;
		left -= nb_chars;
	}

out:
	return count - left;
}

/*
 * Write to a file.
 */
int minix_file_write(struct file_t *filp, const char *buf, int count)
{
	struct super_block_t *sb = filp->f_inode->i_sb;
	size_t pos, nb_chars, left;
	struct buffer_head_t *bh;

	/* handle append flag */
	if (filp->f_flags & O_APPEND)
		pos = filp->f_inode->i_size;
	else
		pos = filp->f_pos;

	left = count;
	while (left > 0) {
		/* read block */
		bh = minix_bread(filp->f_inode, filp->f_pos / sb->s_blocksize, 1);
		if (!bh)
			goto out;

		/* find position and number of chars to read */
		pos = filp->f_pos % sb->s_blocksize;
		nb_chars = sb->s_blocksize - pos <= left ? sb->s_blocksize - pos : left;

		/* copy into buffer */
		memcpy(bh->b_data + pos, buf, nb_chars);

		/* release block */
		bh->b_dirt = 1;
		brelse(bh);

		/* update sizes */
		filp->f_pos += nb_chars;
		buf += nb_chars;
		left -= nb_chars;

		/* end of file : grow it and mark inode dirty */
		if (filp->f_pos > filp->f_inode->i_size) {
			filp->f_inode->i_size = filp->f_pos;
			filp->f_inode->i_dirt = 1;
		}
	}

out:
	return count - left;
}
