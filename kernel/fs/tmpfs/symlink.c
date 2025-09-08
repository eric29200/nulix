#include <fs/tmp_fs.h>
#include <mm/highmem.h>
#include <fcntl.h>
#include <stderr.h>

/*
 * Resolve a symbolic link.
 */
int tmpfs_follow_link(struct inode *dir, struct inode *inode, int flags, mode_t mode, struct inode **res_inode)
{
	struct dentry *dentry;
	struct page *page;
	char *kaddr;

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
	if (list_empty(&inode->u.tmp_i.i_pages)) {
		iput(inode);
		return -EIO;
	}

	/* get first page */
	page = list_first_entry(&inode->u.tmp_i.i_pages, struct page, list);

	/* release link inode */
	iput(inode);

	/* resolve path */
	kaddr = kmap(page);
	dentry = open_namei(AT_FDCWD, dir, kaddr, flags, mode);
	kunmap(page);

	/* handle error */
	if (IS_ERR(dentry))
		return PTR_ERR(dentry);

	/* get result inode */
	*res_inode = dentry->d_inode;
	(*res_inode)->i_count++;

	dput(dentry);
	return 0;
}

/*
 * Read value of a symbolic link.
 */
ssize_t tmpfs_readlink(struct inode *inode, char *buf, size_t bufsize)
{
	struct page *page;
	void *kaddr;

	/* inode must be link */
	if (!S_ISLNK(inode->i_mode))
		return -EINVAL;

	/* limit buffer size to page size */
	if (bufsize > PAGE_SIZE)
		bufsize = PAGE_SIZE;

	/* check first page */
	if (list_empty(&inode->u.tmp_i.i_pages))
		return 0;

	/* get first page */
	page = list_first_entry(&inode->u.tmp_i.i_pages, struct page, list);

	/* copy target name to user buffer */
	kaddr = kmap(page);
	memcpy(buf, kaddr, bufsize);
	kunmap(page);

	return bufsize;
}
