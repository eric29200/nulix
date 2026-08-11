#include <fs/fs.h>
#include <fs/v9fs_fs.h>
#include <mm/mm.h>
#include <stderr.h>

/*
 * Add a fid to a dentry.
 */
int v9fs_fid_add(struct dentry *dentry, struct p9_fid *fid)
{
	struct v9fs_dentry *dent = dentry->d_private;

	/* allocate a 9p dentry if needed */
	if (!dent) {
		dent = (struct v9fs_dentry *) kmalloc(sizeof(struct v9fs_dentry));
		if (!dent)
			return -ENOMEM;

		INIT_LIST_HEAD(&dent->fidlist);
		dentry->d_private = dent;
	}

	/* add fid to dentry */
	list_add(&fid->dlist, &dent->fidlist);

	return 0;
}