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

/*
 * Make a new name for a file = hard link.
 */
int v9fs_link(struct inode *inode, struct inode *dir, struct dentry *dentry)
{
	struct dentry *dir_dentry, *old_dentry;
	struct p9_fid *dfid, *oldfid;

	/* get directory fid */
	dir_dentry = v9fs_dentry_from_dir_inode(dir);
	dfid = v9fs_fid_lookup(dir_dentry);
	if (IS_ERR(dfid))
		return PTR_ERR(dfid);

	/* get old fid */
	old_dentry = v9fs_dentry_from_dir_inode(inode);
	oldfid = v9fs_fid_lookup(old_dentry);
	if (IS_ERR(oldfid))
		return PTR_ERR(oldfid);

	/* issue link request */
	return p9_client_link(dfid, oldfid, dentry->d_name.name);
}

/*
 * Create a symbolic link.
 */
int v9fs_symlink(struct inode *dir, struct dentry *dentry, const char *target)
{
	struct p9_fid *dfid;
	struct p9_qid qid;
	gid_t gid;

	/* get gid for create */
	gid = dir->i_mode & S_ISGID ? dir->i_gid : current_task->fsgid;

	/* get parent directory fid */
	dfid = v9fs_parent_fid(dentry);
	if (IS_ERR(dfid))
		return PTR_ERR(dfid);

	/* issue symlink request */
	return p9_client_symlink(dfid, dentry->d_name.name, target, gid, &qid);
}

/*
 * Remvove a file/directory.
 */
static int v9fs_remove(struct inode *dir, struct dentry *dentry, int rmdir)
{
	struct inode *inode = dentry->d_inode;
	struct p9_fid *fid;
	int ret;

	/* get fid */
	fid = v9fs_fid_clone(dentry);
	if (IS_ERR(fid))
		return PTR_ERR(fid);

	/* remove request */
	ret = p9_client_remove(fid);
	if (ret)
		return ret;

	/* directories on unlink should have zero link count */
	if (rmdir) {
		inode->i_nlinks = 0;
		dir->i_nlinks--;
	} else {
		inode->i_nlinks--;
	}

	return 0;
}

/*
 * Remove a file.
 */
int v9fs_unlink(struct inode *dir, struct dentry *dentry)
{
	return v9fs_remove(dir, dentry, 0);
}

/*
 * Remove a directory.
 */
int v9fs_rmdir(struct inode *dir, struct dentry *dentry)
{
	return v9fs_remove(dir, dentry, 1);
}

/*
 * Rename a file.
 */
int v9fs_rename(struct inode *old_dir, struct dentry *old_dentry, struct inode *new_dir, struct dentry *new_dentry)
{
	struct inode *old_inode = old_dentry->d_inode, *new_inode = new_dentry->d_inode;
	struct p9_fid *oldfid, *olddirfid, *newdirfid;
	int ret;

	/* get old fid */
	oldfid = v9fs_fid_lookup(old_dentry);
	if (IS_ERR(oldfid))
		return PTR_ERR(oldfid);

	/* get old directory fid */
	olddirfid = v9fs_fid_clone(old_dentry->d_parent);
	if (IS_ERR(olddirfid))
		return PTR_ERR(olddirfid);

	/* get new directory fid */
	newdirfid = v9fs_fid_clone(new_dentry->d_parent);
	if (IS_ERR(newdirfid)) {
		ret = PTR_ERR(newdirfid);
		goto out_clunk_olddir;
	}

	/* issue rename request */
	ret = p9_client_rename(oldfid, newdirfid, new_dentry->d_name.name);
	if (ret)
		goto out_clunk_newdir;

	/* update new inode */
	if (new_inode) {
		if (S_ISDIR(new_inode->i_mode))
			new_inode->i_nlinks = 0;
		else
			new_inode->i_nlinks--;
	}

	/* update old inode */
	if (S_ISDIR(old_inode->i_mode)) {
		if (!new_inode)
			new_dir->i_nlinks++;
		old_dir->i_nlinks = 0;
	}

	/* successful rename */
	d_move(old_dentry, new_dentry);
out_clunk_newdir:
	p9_client_clunk(newdirfid);
out_clunk_olddir:
	p9_client_clunk(olddirfid);
	return ret;
}