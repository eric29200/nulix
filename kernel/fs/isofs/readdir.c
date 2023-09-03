#include <fs/fs.h>
#include <fs/iso_fs.h>
#include <mm/mm.h>
#include <string.h>
#include <stderr.h>

/*
 * Get directory entries system call.
 */
int isofs_getdents64(struct file *filp, void *dirp, size_t count)
{
	struct isofs_inode_info *isofs_inode = &filp->f_inode->u.iso_i;
	char name[ISOFS_MAX_NAME_LEN + 1], de_tmp[4096];
	struct super_block *sb = filp->f_inode->i_sb;
	int de_len, entries_size = 0, name_len, ret;
	struct inode *inode = filp->f_inode;
	uint32_t offset, next_offset, block;
	struct iso_directory_record *de;
	struct dirent64 *dirent;
	struct buffer_head *bh;
	ino_t ino;

	/* init */
	dirent = (struct dirent64 *) dirp;

	/* compute position */
	offset = filp->f_pos & (sb->s_blocksize - 1);
	block = (isofs_inode->i_first_extent >> sb->s_blocksize_bits) + (filp->f_pos >> sb->s_blocksize_bits);
	if (!block)
		return entries_size;

	/* read first block */
	bh = bread(sb->s_dev, block, sb->s_blocksize);
	if (!bh)
		return entries_size;

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
				return entries_size;

			/* read next block */
			bh = bread(sb->s_dev, block, sb->s_blocksize);
			if (!bh)
				return entries_size;
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
				return entries_size;

			/* read next block */
			bh = bread(sb->s_dev, block, sb->s_blocksize);
			if (!bh)
				return entries_size;

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
			bh = bread(sb->s_dev, block, sb->s_blocksize);
			if (!bh)
				return entries_size;

			/* copy 2nd fragment */
			memcpy(&de_tmp[sb->s_blocksize - offset], bh->b_data, next_offset);
		}
		offset = next_offset;

		/* fake "." entry */
		if (de->name_len[0] == 1 && de->name[0] == 0) {
			ret = filldir(dirent, ".", 1, inode->i_ino, count);
			if (ret) {
				brelse(bh);
				return entries_size;
			}

			goto next;
		}

		/* fake ".." entry */
		if (de->name_len[0] == 1 && de->name[0] == 1) {
			ret = filldir(dirent, "..", 2, inode->i_ino, count);
			if (ret) {
				brelse(bh);
				return entries_size;
			}

			goto next;
		}

		/* translate name */
		name_len = isofs_name_translate(de->name, de->name_len[0], name);

		/* fill in directory entry */ 
		ret = filldir(dirent, name, name_len, ino, count);
		if (ret) {
			brelse(bh);
			return entries_size;
		}

		/* go to next entry */
next:
		count -= dirent->d_reclen;
		entries_size += dirent->d_reclen;
		dirent = (struct dirent64 *) ((char *) dirent + dirent->d_reclen);

		/* update file position */
		filp->f_pos += de_len;
	}

	return entries_size;
}
