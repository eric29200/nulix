#include <fs/fs.h>
#include <fs/v9fs_fs.h>
#include <stdio.h>
#include <stderr.h>
#include <fcntl.h>

/*
 * Open a file (or directory).
 */
int v9fs_open(struct inode *inode, struct file *filp)
{
	struct p9_fid *fid = filp->f_private;
	int ret;

	/* get a fid if needed */
	if (!fid) {
		/* get fid */
		fid = v9fs_fid_clone(filp->f_dentry);
		if (IS_ERR(fid))
			return PTR_ERR(fid);

		/* open file on server */
		ret = p9_client_open(fid, filp->f_flags);
		if (ret < 0) {
			p9_client_clunk(fid);
			return ret;
		}

		/* truncate file */
		if (filp->f_flags & O_TRUNC) {
			inode->i_size = 0;
			inode->i_blocks = 0;
		}
	}

	/* attach fid to file */
	filp->f_private = fid;

	return 0;
}