#include <fs/fs.h>
#include <fs/minix_fs.h>
#include <string.h>
#include <fcntl.h>

/*
 * Read a file.
 */
int minix_file_read(struct file *filp, char *buf, int count)
{
	struct super_block *sb = filp->f_inode->i_sb;
	size_t pos, nb_chars, left;
	struct buffer_head *bh;

	/* adjust size */
	if (filp->f_pos + count > filp->f_inode->i_size)
		count = filp->f_inode->i_size - filp->f_pos;

	if (count <= 0)
		return 0;

	left = count;
	while (left > 0) {
		/* get block */
		bh = minix_getblk(filp->f_inode, filp->f_pos / sb->s_blocksize, 0);
		if (!bh)
			break;

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

	return count - left;
}

/*
 * Write to a file.
 */
int minix_file_write(struct file *filp, const char *buf, int count)
{
	struct super_block *sb = filp->f_inode->i_sb;
	size_t pos, nb_chars, left;
	struct buffer_head *bh;

	/* handle append flag */
	if (filp->f_flags & O_APPEND)
		filp->f_pos = filp->f_inode->i_size;

	left = count;
	while (left > 0) {
		/* get/create block */
		bh = minix_getblk(filp->f_inode, filp->f_pos / sb->s_blocksize, 1);
		if (!bh)
			break;

		/* find position and number of chars to read */
		pos = filp->f_pos % sb->s_blocksize;
		nb_chars = sb->s_blocksize - pos <= left ? sb->s_blocksize - pos : left;

		/* copy into buffer */
		memcpy(bh->b_data + pos, buf, nb_chars);

		/* release block */
		bh->b_dirt = 1;
		brelse(bh);

		/* update page cache */
		update_vm_cache(filp->f_inode, buf, pos, nb_chars);

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

	return count - left;
}
