#include <fs/fs.h>
#include <fs/cramfs_fs.h>
#include <stdio.h>
#include <stderr.h>

/*
 * Read directory.
 */
int cramfs_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	struct inode *inode = filp->f_dentry->d_inode;
	struct super_block *sb = inode->i_sb;
	uint32_t offset, next_offset;
	struct cramfs_inode *de;
	int name_len;
	char *name;

	/* get offset */
	offset = filp->f_pos;
	if (offset >= inode->i_size)
		return 0;

	/* directories entries are always 4 bytes aligned */
	if (offset & 3)
		return -EINVAL;

	/* read block by block */
	while (offset < inode->i_size) {
		de = cramfs_read(sb, CRAMOFFSET(inode) + offset, sizeof(*de) + 256);
		name = (char *) (de + 1);

		 /* namelengths on disk are shifted by two and the name padded out to 4-byte boundaries with zeroes */
		name_len = de->namelen << 2;
		next_offset = offset + sizeof(*de) + name_len;
		for (;;) {
			if (!name_len)
				return -EIO;
			if (name[name_len - 1])
				break;
			name_len--;
		}

		/* fill in directory entry */
		if (filldir(dirent, name, name_len, offset, cramino(de, CRAMOFFSET(inode) + offset)))
			break;

		/* go to next entry */
		offset = next_offset;
		filp->f_pos = offset;
	}

	return 0;
}