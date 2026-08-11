#include <fs/fs.h>
#include <fs/v9fs_fs.h>
#include <stdio.h>
#include <stderr.h>

/*
 * Lookup for a file in a directory.
 */
struct dentry *v9fs_lookup(struct inode *dir, struct dentry *dentry)
{
	struct p9_fid *d_fid, *fid;
	struct inode *inode;
	char *name;
	int ret;

	/* get parent fid */
	d_fid = v9fs_fid_lookup(dentry->d_parent);
	if (IS_ERR(d_fid))
		return ERR_CAST(d_fid);

	/* walk directory */
	name = (char *) dentry->d_name.name;
	fid = p9_client_walk(d_fid, 1, &name, 1);
	if (IS_ERR(fid)) {
		ret = PTR_ERR(fid);
		if (ret == -ENOENT) {
			inode = NULL;
			goto out;
		}

		return ERR_PTR(ret);
	}

	/* get an inode from fid */
	inode = v9fs_get_inode_from_fid(fid, dir->i_sb);
	if (IS_ERR(inode)) {
		ret = PTR_ERR(inode);
		inode = NULL;
		goto err;
	}

	/* add fid to dentry */
	ret = v9fs_fid_add(dentry, fid);
	if (ret < 0)
		goto err_iput;

out:
	d_add(dentry, inode);
	return NULL;
err_iput:
	iput(inode);
err:
	p9_client_clunk(fid);
	return ERR_PTR(ret);
}