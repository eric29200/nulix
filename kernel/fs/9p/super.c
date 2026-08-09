#include <fs/fs.h>
#include <fs/v9fs_fs.h>
#include <net/9p/9p.h>
#include <stderr.h>
#include <stdio.h>

/*
 * Close a session.
 */
static void v9fs_session_close(struct v9fs_session_info *v9ses)
{
	UNUSED(v9ses);
	printf("TODO: v9fs_session_close\n");
}

/*
 * Init a session.
 */
static struct p9_fid *v9fs_session_init(struct v9fs_session_info *v9ses, const char *dev_name, char *data)
{
	UNUSED(v9ses);
	UNUSED(dev_name);
	UNUSED(data);
	printf("TODO: v9fs_session_init\n");
	return ERR_PTR(-EINVAL);
}

/*
 * Read super block.
 */
static struct super_block *v9fs_read_super(struct super_block *sb, const char *dev_name, void *data, int silent)
{
	struct v9fs_session_info *v9ses;
	struct p9_fid *fid;

	/* allocate a new session */
	sb->s_fs_info = v9ses = (struct v9fs_session_info *) kmalloc(sizeof(struct v9fs_session_info));
	if (!v9ses)
		return NULL;

	/* init session */
	fid = v9fs_session_init(v9ses, dev_name, data);
	if (IS_ERR(fid))
		goto err_close_session;

	printf("TODO: v9fs_read_super\n");
	return NULL;
err_close_session:
	if (!silent)
		p9_error("Can't init session\n");
	v9fs_session_close(v9ses);
	kfree(v9ses);
	return NULL;
}

/*
 * 9p file system.
 */
static struct file_system_type v9fs_fs = {
	.name			= "9p",
	.read_super		= v9fs_read_super,
};

/*
 * Init 9p file system.
 */
int init_v9fs_fs()
{
	return register_filesystem(&v9fs_fs);
}
