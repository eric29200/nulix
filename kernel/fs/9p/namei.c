#include <fs/fs.h>
#include <fs/v9fs_fs.h>
#include <proc/sched.h>
#include <stdio.h>
#include <stderr.h>
#include <fcntl.h>

/*
 * Lookup for a file in a directory.
 */
struct dentry *v9fs_lookup(struct inode *dir, struct dentry *dentry)
{
	struct p9_fid *d_fid, *fid;
	struct inode *inode;
	char *name;
	int ret;

	/* set dentry operations */
	dentry->d_op = &v9fs_dops;

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

/*
 * Create a file in a directory.
 */
int v9fs_create(struct inode *dir, struct dentry *dentry, mode_t mode)
{
	return v9fs_mknod(dir, dentry, mode, 0);
}

/*
 * Create a node.
 */
int v9fs_mknod(struct inode *dir, struct dentry *dentry, mode_t mode, dev_t dev)
{
	char *name = dentry->d_name.name;
	struct p9_fid *dfid;
	struct p9_qid qid;
	gid_t gid;

	/* fix mode */
	if (dir->i_mode & S_ISGID)
		mode |= S_ISGID;
	mode &= ~current_task->fs->umask;

	/* get gid for create */
	gid = dir->i_mode & S_ISGID ? dir->i_gid : current_task->fsgid;

	/* get parent directory fid */
	dfid = v9fs_parent_fid(dentry);
	if (IS_ERR(dfid))
		return PTR_ERR(dfid);

	/* issue mknod request */
	return p9_client_mknod(dfid, name, mode, dev, gid, &qid);
}

/*
 * Create a directory.
 */
int v9fs_mkdir(struct inode *dir, struct dentry *dentry, mode_t mode)
{
	char *name = dentry->d_name.name;
	struct p9_fid *dfid;
	struct p9_qid qid;
	gid_t gid;

	/* fix mode */
	mode |= S_IFDIR;
	if (dir->i_mode & S_ISGID)
		mode |= S_ISGID;
	mode &= ~current_task->fs->umask;

	/* get gid for create */
	gid = dir->i_mode & S_ISGID ? dir->i_gid : current_task->fsgid;

	/* get parent directory fid */
	dfid = v9fs_parent_fid(dentry);
	if (IS_ERR(dfid))
		return PTR_ERR(dfid);

	/* issue mkdir request */
	return p9_client_mkdir(dfid, name, mode, gid, &qid);
}