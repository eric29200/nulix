#include <fs/proc_fs.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stderr.h>

/*
 * Read a link.
 */
int do_proc_readlink(struct dentry *dentry, char *buf, size_t bufsize)
{
	struct inode *inode = dentry->d_inode;
	char *page, *pattern = NULL, *path;
	size_t len;
	int err;

	/* get a free page */
	page = get_free_page();
	if (!page)
		return -ENOMEM;

	/* special dentries */
	if (inode && dentry->d_parent == dentry) {
		if (S_ISSOCK(inode->i_mode))
			pattern = "socket:[%lu]";
		if (S_ISFIFO(inode->i_mode))
			pattern = "pipe:[%lu]";
	}

	if (pattern) {
		len = sprintf(page, pattern, inode->i_ino);
		path = page;
	} else {
		path = d_path(dentry, page, PAGE_SIZE, &err);
		len = page + PAGE_SIZE - 1 - path;
	}

	if (bufsize < len)
		len = bufsize;

	/* copy result */
	memcpy(buf, path, len);

	return len;
}