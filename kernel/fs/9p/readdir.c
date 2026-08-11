#include <fs/fs.h>
#include <fs/v9fs_fs.h>
#include <stdio.h>
#include <stderr.h>

/*
 * Read directory buffer.
 */
struct p9_rdir {
	int		head;
	int		tail;
	char *		buf;
};

/*
 * Allocate buffer used for read and readdir.
 */
static int v9fs_alloc_rdir_buf(struct file *filp, int buf_len)
{
	struct p9_fid *fid = filp->f_private;
	struct p9_rdir *rdir;

	/* already allocated */
	if (fid->rdir)
		return 0;

	/* allocated read buffer */
	rdir = (struct p9_rdir *) kmalloc(sizeof(struct p9_rdir) + buf_len);
	if (!rdir)
		return -ENOMEM;

	/* set read buffer */
	rdir->buf = (char *) rdir + sizeof(struct p9_rdir);
	rdir->head = rdir->tail = 0;
	fid->rdir = (void *) rdir;

	return 0;
}

/*
 * Read directory.
 */
int v9fs_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	struct p9_fid *fid = filp->f_private;
	struct p9_dirent p9_dirent;
	int buf_len, over, ret = 0;
	struct p9_rdir *rdir;
	off_t old_offset = 0;

	/* allocate read buffer */
	buf_len = fid->client->msize - P9_IOHDRSZ;
	ret = v9fs_alloc_rdir_buf(filp, buf_len);
	if (ret)
		return ret;

	rdir = (struct p9_rdir *) fid->rdir;
	for (;;) {
		/* read next entries if needed */
		if (rdir->tail == rdir->head) {
			ret = p9_client_readdir(fid, (char *) rdir->buf, buf_len, filp->f_pos);
			if (ret <= 0)
				return ret;

			rdir->head = 0;
			rdir->tail = ret;
		}

		/* parse entries */
		while (rdir->head < rdir->tail) {
			/* parse entry */
			ret = p9dirent_read(rdir->buf + rdir->head, rdir->tail - rdir->head, &p9_dirent);
			if (ret < 0)
				return -EIO;

			/* fill in dir entry */
			over = filldir(dirent, p9_dirent.d_name, strlen(p9_dirent.d_name), old_offset, v9fs_qid2ino(&p9_dirent.qid));
			old_offset = p9_dirent.d_off;
			if (over)
				return 0;

			/* go to next entry */
			filp->f_pos = p9_dirent.d_off;
			rdir->head += ret;
		}

		break;
	}

	return 0;
}