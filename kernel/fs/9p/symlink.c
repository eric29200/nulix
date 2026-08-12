#include <fs/fs.h>
#include <fs/v9fs_fs.h>
#include <stderr.h>
#include <fcntl.h>

/*
 * Follow a link.
 */
struct dentry *v9fs_follow_link(struct dentry *dentry, struct dentry *base)
{
	struct p9_fid *fid;
	struct dentry *res;
	char *target;
	int ret;

	/* lookup for fid */
	fid = v9fs_fid_lookup(dentry);
	if (IS_ERR(fid))
		return ERR_CAST(fid);

	/* issue read link request */
	ret = p9_client_readlink(fid, &target);
	if (ret)
		return ERR_PTR(ret);

	/* lookup target */
	res = lookup_dentry(AT_FDCWD, base, target, 1);

	/* free target */
	kfree(target);

	return res;
}

/*
 * Read value of a symbolic link.
 */
ssize_t v9fs_readlink(struct dentry *dentry, char *buf, size_t bufsize)
{
	struct inode *inode = dentry->d_inode;
	struct p9_fid *fid;
	char *target;
	size_t len;
	int ret;

	/* inode must be a link */
	if (!S_ISLNK(inode->i_mode))
		return -EINVAL;

	/* lookup for fid */
	fid = v9fs_fid_lookup(dentry);
	if (IS_ERR(fid))
		return PTR_ERR(fid);

	/* issue read link request */
	ret = p9_client_readlink(fid, &target);
	if (ret)
		return ret;

	/* limit length */
	len = strlen(target);
	if (len > bufsize)
		len = bufsize;

	/* copy target */
	memcpy(buf, target, len);

	/* free target */
	kfree(target);

	return len;
}