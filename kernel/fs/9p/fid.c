#include <fs/fs.h>
#include <fs/v9fs_fs.h>
#include <proc/sched.h>
#include <mm/mm.h>
#include <stderr.h>

#define min(x, y)		((x) <= (y) ? (x) : (y))

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
 * Build path from a dentry.
 */
static int build_path_from_dentry(struct dentry *dentry, char ***names)
{
	struct dentry *ds;
	char **wnames;
	int n = 0, i;

	/* find number of elements */
	for (ds = dentry; ds != ds->d_parent; ds = ds->d_parent)
		n++;

	/* allocate names */
	wnames = (char **) kmalloc(sizeof(char *) * n);
	if (!wnames)
		return -ENOMEM;

	/* set names */
	for (ds = dentry, i = n-1; i >= 0; i--, ds = ds->d_parent)
		wnames[i] = (char *) ds->d_name.name;

	*names = wnames;
	return n;
}

/*
 * Lookup for a fid, try to walk if not found.
 */
static struct p9_fid *v9fs_fid_lookup_with_uid(struct dentry *dentry, uid_t uid, int any)
{
	struct v9fs_session_info *v9ses = dentry->d_sb->s_fs_info;
	struct p9_fid *fid, *old_fid = NULL;
	int clone = 1, n, l, i;
	char **wnames;

	/* try to find fid in dentry */
	fid = v9fs_fid_find(dentry, uid, any);
	if (fid)
		return fid;

	/* walk parent */
	fid = v9fs_fid_find(dentry->d_parent, uid, any);
	if (fid) {
		/* Found the parent fid do a lookup with that */
		fid = p9_client_walk(fid, 1, (char **) &dentry->d_name.name, 1);
		goto out;
	}

	/* start from the root and try to do a lookup */
	fid = v9fs_fid_find(dentry->d_sb->s_root, uid, any);
	if (!fid) {
		/* attach to server if needed */
		fid = p9_client_attach(v9ses->client, NULL, v9ses->uname, uid, v9ses->aname);
		if (IS_ERR(fid))
			return fid;

		v9fs_fid_add(dentry->d_sb->s_root, fid);
	}

	/* if we are root ourself just return that */
	if (dentry->d_sb->s_root == dentry)
		return fid;

	/* do a multipath walk with attached root */
	n = build_path_from_dentry(dentry, &wnames);
	if (n < 0)
		return ERR_PTR(n);

	for (i = 0; i < n; ) {
		l = min(n - i, P9_MAXWELEM);

		fid = p9_client_walk(fid, l, &wnames[i], clone);
		if (IS_ERR(fid)) {
			if (old_fid)
				p9_client_clunk(old_fid);
			kfree(wnames);
			return fid;
		}

		old_fid = fid;
		i += l;
		clone = 0;
	}

	/* free names */
	kfree(wnames);
out:
	if (!IS_ERR(fid))
		v9fs_fid_add(dentry, fid);
	return fid;
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

/*
 * Get parent fid.
 */
struct p9_fid *v9fs_parent_fid(struct dentry *dentry)
{
	return v9fs_fid_lookup(dentry->d_parent);
}
