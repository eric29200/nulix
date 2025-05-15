#include <fs/fs.h>
#include <fs/iso_fs.h>
#include <string.h>
#include <fcntl.h>

/*
 * Read a file.
 */
int isofs_file_read(struct file *filp, char *buf, int count)
{
	struct super_block *sb = filp->f_inode->i_sb;
	size_t pos, nr_chars, left;
	struct buffer_head *bh;

	/* adjust size */
	if (filp->f_pos + count > filp->f_inode->i_size)
		count = filp->f_inode->i_size - filp->f_pos;

	if (count <= 0)
		return 0;

	left = count;
	while (left > 0) {
		/* get block */
		bh = isofs_getblk(filp->f_inode, filp->f_pos / sb->s_blocksize, 0);
		if (!bh)
			break;

		/* find position and number of chars to read */
		pos = filp->f_pos % sb->s_blocksize;
		nr_chars = sb->s_blocksize - pos <= left ? sb->s_blocksize - pos : left;

		/* copy into buffer */
		memcpy(buf, bh->b_data + pos, nr_chars);

		/* release block */
		brelse(bh);

		/* update sizes */
		filp->f_pos += nr_chars;
		buf += nr_chars;
		left -= nr_chars;
	}

	return count - left;
}
