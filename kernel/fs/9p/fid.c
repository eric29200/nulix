#include <fs/fs.h>
#include <fs/v9fs_fs.h>
#include <proc/sched.h>
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

		INIT_LIST_HEAD(&dent->fid_list);
		dentry->d_private = dent;
	}

	/* add fid to dentry */
	list_add(&fid->dlist, &dent->fid_list);

	return 0;
}

/*
 * Retrieve a fid that belongs to the specified uid.
 *
 */
static struct p9_fid *v9fs_fid_find(struct dentry *dentry, uint32_t uid, int any)
{
	struct v9fs_dentry *dent = (struct v9fs_dentry *) dentry->d_private;
	struct list_head *pos;
	struct p9_fid *fid;

	if (!dent)
		return NULL;

	list_for_each(pos, &dent->fid_list) {
		fid = list_entry(pos, struct p9_fid, dlist);
		if (any || fid->uid == uid)
			return fid;
	}

	return NULL;
}

/*
 * Lookup for a fid, try to walk if not found.
 */
static struct p9_fid *v9fs_fid_lookup_with_uid(struct dentry *dentry, uid_t uid, int any)
{
	struct p9_fid *fid;

	/* try to find fid in dentry */
	fid = v9fs_fid_find(dentry, uid, any);
	if (fid)
		return fid;

	printf("TODO: v9fs_fid_lookup_with_uid\n");
	return ERR_PTR(-EINVAL);
}

/*
 * Lookup for a fid, try to walk if not found.
 */
struct p9_fid *v9fs_fid_lookup(struct dentry *dentry)
{
	return v9fs_fid_lookup_with_uid(dentry, current_task->uid, 1);
}

/*
 * Clone a fid.
 */
struct p9_fid *v9fs_fid_clone(struct dentry *dentry)
{
	struct p9_fid *fid;

	/* lookup for fid in dentry */
	fid = v9fs_fid_lookup(dentry);
	if (IS_ERR(fid))
		return fid;

	/* do a twalk */
	return p9_client_walk(fid, 0, NULL, 1);
}