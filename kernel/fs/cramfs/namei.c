#include <fs/fs.h>
#include <fs/cramfs_fs.h>
#include <stdio.h>
#include <stderr.h>

/*
 * Lookup for a file in a directory.
 */
struct dentry *cramfs_lookup(struct inode *dir, struct dentry *dentry)
{
	struct cramfs_inode *de;
	size_t offset = 0;
	size_t name_len;
	int dir_off;
	char *name;

	while (offset < dir->i_size) {
		dir_off = CRAMOFFSET(dir) + offset;

		/* get next entry */
		de = cramfs_read(dir->i_sb, CRAMOFFSET(dir) + offset, sizeof(*de) + 256);
		name = (char *)(de + 1);
		name_len = de->namelen << 2;
		offset += sizeof(*de) + name_len;

		/* check that the name is roughly the right length */
		if (((dentry->d_name.len + 3) & ~3) != name_len)
			continue;

		/* trim name */
		for (;;) {
			if (!name_len)
				return ERR_PTR(-EIO);
			if (name[name_len - 1])
				break;
			name_len--;
		}

		/* find entry */
		if (name_len != dentry->d_name.len)
			continue;
		if (memcmp(dentry->d_name.name, name, name_len))
			continue;

		d_add(dentry, cramfs_get_inode(dir->i_sb, de, dir_off));
		return NULL;
	}

	d_add(dentry, NULL);
	return NULL;
}