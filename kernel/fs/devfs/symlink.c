#include <fs/dev_fs.h>
#include <fcntl.h>
#include <stderr.h>

/*
 * Resolve a symbolic link.
 */
int devfs_follow_link(struct inode_t *dir, struct inode_t *inode, int flags, mode_t mode, struct inode_t **res_inode)
{
	struct page_t *page;
	int ret;

	*res_inode = NULL;

	/* check inode */
	if (!inode)
		return -ENOENT;

	/* not a link */
	if (!S_ISLNK(inode->i_mode)) {
		*res_inode = inode;
		return 0;
	}

	/* check first page */
	if (list_empty(&inode->u.dev_i.i_pages)) {
		iput(inode);
		return -EIO;
	}

	/* get first page */
	page = list_first_entry(&inode->u.dev_i.i_pages, struct page_t, list);

	/* release link inode */
	iput(inode);

	/* open target inode */
	ret = open_namei(AT_FDCWD, dir, (char *) PAGE_ADDRESS(page), flags, mode, res_inode);

	return ret;
}

/*
 * Read value of a symbolic link.
 */
ssize_t devfs_readlink(struct inode_t *inode, char *buf, size_t bufsize)
{
	struct page_t *page;

	/* inode must be link */
	if (!S_ISLNK(inode->i_mode)) {
		iput(inode);
		return -EINVAL;
	}

	/* limit buffer size to page size */
	if (bufsize > PAGE_SIZE)
		bufsize = PAGE_SIZE;

	/* check first page */
	if (list_empty(&inode->u.dev_i.i_pages)) {
		iput(inode);
		return 0;
	}

	/* get first page */
	page = list_first_entry(&inode->u.dev_i.i_pages, struct page_t, list);

	/* release inode */
	iput(inode);

	/* copy target name to user buffer */
	memcpy(buf, (char *) PAGE_ADDRESS(page), bufsize);

	return bufsize;
}
