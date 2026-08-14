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

/*
 * Release a file.
 */
int v9fs_release(struct inode *inode, struct file *filp)
{
	struct p9_fid *fid = filp->f_private;

	UNUSED(inode);

	if (fid)
		p9_client_clunk(fid);

	return 0;
}

/*
 * Read from a fid.
 */
static int v9fs_fid_readn(struct p9_fid *fid, char *buf, uint32_t count, uint64_t offset)
{
	int n = 0, total = 0, r_size;

	/* choose read size */
	r_size = fid->iounit ? fid->iounit : fid->client->msize - P9_IOHDRSZ;

	/* read */
	do {
		n = p9_client_read(fid, buf, offset, count);
		if (n <= 0)
			break;

		buf += n;
		offset += n;
		count -= n;
		total += n;
	} while (count > 0 && n == r_size);

	/* return error */
	if (n < 0)
		total = n;

	return total;
}

/*
 * Read from a file.
 */
int v9fs_file_read(struct file *filp, char *buf, size_t count, off_t *offset)
{
	struct p9_fid *fid = filp->f_private;
	size_t r_size;
	int ret;

	/* choose read size */
	r_size = fid->iounit ? fid->iounit : fid->client->msize - P9_IOHDRSZ;

	/* can be read in one time */
	if (count <= r_size)
		ret = p9_client_read(fid, buf, *offset, count);
	else
		ret = v9fs_fid_readn(fid, buf, count, *offset);

	/* update offset */
	if (ret > 0)
		*offset += ret;

	return ret;
}

/*
 * Write to a fid.
 */
static int v9fs_fid_writen(struct p9_fid *fid, const char *buf, uint32_t count, uint64_t offset)
{
	int n = 0, total = 0, w_size;

	/* choose write size */
	w_size = fid->iounit ? fid->iounit : fid->client->msize - P9_IOHDRSZ;

	/* read */
	do {
		n = p9_client_write(fid, buf, offset, count);
		if (n <= 0)
			break;

		buf += n;
		offset += n;
		count -= n;
		total += n;
	} while (count > 0 && n == w_size);

	/* return error */
	if (n < 0)
		total = n;

	return total;
}

/*
 * Write to a file.
 */
int v9fs_file_write(struct file *filp, const char *buf, size_t count, off_t *offset)
{
	struct p9_fid *fid = filp->f_private;
	size_t w_size;
	int ret;

	/* choose write size */
	w_size = fid->iounit ? fid->iounit : fid->client->msize - P9_IOHDRSZ;

	/* can be written in one time */
	if (count <= w_size)
		ret = p9_client_write(fid, buf, *offset, count);
	else
		ret = v9fs_fid_writen(fid, buf, count, *offset);

	/* update offset */
	if (ret > 0)
		*offset += ret;

	return ret;
}