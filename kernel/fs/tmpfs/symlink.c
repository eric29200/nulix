#include <fs/tmp_fs.h>
#include <mm/highmem.h>
#include <fcntl.h>
#include <stderr.h>

/*
 * Resolve a symbolic link.
 */
struct dentry *tmpfs_follow_link(struct dentry *dentry, struct dentry *base)
{
	struct inode *inode = dentry->d_inode;
	struct dentry *res;
	struct page *page;
	char *kaddr;

	/* check first page */
	if (list_empty(&inode->u.tmp_i.i_pages)) {
		dput(base);
		return ERR_PTR(-EIO);
	}

	/* get first page */
	page = list_first_entry(&inode->u.tmp_i.i_pages, struct page, list);

	/* resolve target */
	kaddr = kmap(page);
	res = lookup_dentry(AT_FDCWD, base, kaddr, 1);
	kunmap(page);

	return res;
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
