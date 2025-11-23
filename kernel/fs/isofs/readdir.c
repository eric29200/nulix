#include <fs/fs.h>
#include <fs/iso_fs.h>
#include <mm/mm.h>
#include <string.h>
#include <stderr.h>

/*
 * Read directory.
 */
int isofs_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	struct isofs_inode_info *isofs_inode = &filp->f_dentry->d_inode->u.iso_i;
	struct super_block *sb = filp->f_dentry->d_inode->i_sb;
	char name[ISOFS_MAX_NAME_LEN + 1], de_tmp[4096];
	struct inode *inode = filp->f_dentry->d_inode;
	uint32_t offset, next_offset, block;
	struct iso_directory_record *de;
	struct buffer_head *bh;
	int de_len, name_len;
	ino_t ino;

	/* compute position */
	offset = filp->f_pos & (sb->s_blocksize - 1);
	block = (isofs_inode->i_first_extent >> sb->s_blocksize_bits) + (filp->f_pos >> sb->s_blocksize_bits);
	if (!block)
		return 0;

	/* read first block */
	bh = bread(inode->i_dev, block, sb->s_blocksize);
	if (!bh)
		return 0;

	/* walk through directory */
	while (filp->f_pos < inode->i_size) {
		/* read next block */
		if (offset >= sb->s_blocksize) {
			/* release previous block buffer */
			brelse(bh);

			/* compute next block */
			offset = 0;
			block = (isofs_inode->i_first_extent >> sb->s_blocksize_bits) + (filp->f_pos >> sb->s_blocksize_bits);
			if (!block)
				return 0;

			/* read next block */
			bh = bread(inode->i_dev, block, sb->s_blocksize);
			if (!bh)
				return 0;
		}

		/* get directory entry and compute inode */
		de = (struct iso_directory_record *) (bh->b_data + offset);
		ino = (block << sb->s_blocksize_bits) + (offset & (sb->s_blocksize - 1));
		de_len = *((unsigned char *) de);

		/* zero entry : go to next sector */
		if (!de_len) {
			/* release previous block buffer */
			brelse(bh);

			/* update file position */
			filp->f_pos = ((filp->f_pos & ~(sb->s_blocksize - 1)) + sb->s_blocksize);

			/* compute next block */
			offset = 0;
			block = (isofs_inode->i_first_extent >> sb->s_blocksize_bits) + (filp->f_pos >> sb->s_blocksize_bits);
			if (!block)
				return 0;

			/* read next block */
			bh = bread(inode->i_dev, block, sb->s_blocksize);
			if (!bh)
				return 0;

			continue;
		}

		/* directory entry is on 2 blocks */
		next_offset = offset + de_len;
		if (next_offset > sb->s_blocksize) {
			/* copy first fragment */
			next_offset &= (sb->s_blocksize - 1);
			memcpy(de_tmp, de, sb->s_blocksize - offset);

			/* read next block buffer */
			brelse(bh);
			block = (isofs_inode->i_first_extent >> sb->s_blocksize_bits) + ((filp->f_pos + de_len) >> sb->s_blocksize_bits);
			bh = bread(inode->i_dev, block, sb->s_blocksize);
			if (!bh)
				return 0;

			/* copy 2nd fragment */
			memcpy(&de_tmp[sb->s_blocksize - offset], bh->b_data, next_offset);
		}
		offset = next_offset;

		/* fake "." entry */
		if (de->name_len[0] == 1 && de->name[0] == 0) {
			if (filldir(dirent, ".", 1, filp->f_pos, inode->i_ino)) {
				brelse(bh);
				return 0;
			}

			goto next;
		}

		/* fake ".." entry */
		if (de->name_len[0] == 1 && de->name[0] == 1) {
			if (filldir(dirent, "..", 2, filp->f_pos, inode->i_ino)) {
				brelse(bh);
				return 0;
			}

			goto next;
		}

		/* translate name */
		name_len = get_rock_ridge_filename(de, name, inode);
		if (!name_len)
			name_len = isofs_name_translate(de->name, de->name_len[0], name);

		/* fill in directory entry */
		if (filldir(dirent, name, name_len, filp->f_pos, ino)) {
			brelse(bh);
			return 0;
		}

next:
		filp->f_pos += de_len;
	}

	return 0;
}
