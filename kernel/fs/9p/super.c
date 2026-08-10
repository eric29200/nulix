#include <fs/fs.h>
#include <fs/v9fs_fs.h>
#include <net/9p/9p.h>
#include <lib/parser.h>
#include <stderr.h>
#include <stdio.h>
#include <fcntl.h>

/*
 * Options.
 */
enum {
	Opt_uname,
	Opt_remotename,
	Opt_err
};

static const struct match_token tokens[] = {
	{ Opt_uname,		"uname=%s" 	},
	{ Opt_remotename,	"aname=%s" 	},
	{ Opt_err, 		NULL 		},
};

/*
 * Parse options.
 */
static int v9fs_parse_options(struct v9fs_session_info *v9ses, char *opts)
{
	struct substring args[MAX_OPT_ARGS];
	char *options, *tmp_options, *p;
	int token;

	/* no options */
	if (!opts)
		return 0;

	/* dup options */
	tmp_options = strdup(opts);
	if (!tmp_options)
		return -ENOMEM;
	options = tmp_options;

	/* parse options */
	for (;;) {
		p = strsep(&options, ",");
		if (!p)
			break;
		if (!*p)
			continue;

		/* get next option */
		token = match_token(p, tokens, args);

		/* parse option */
		switch (token) {
			case Opt_uname:
				match_strlcpy(v9ses->uname, &args[0], PATH_MAX);
				break;
			case Opt_remotename:
				match_strlcpy(v9ses->aname, &args[0], PATH_MAX);
				break;
			default:
				continue;
		}
	}

	kfree(tmp_options);
	return 0;
}

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
	struct p9_fid *fid;
	int ret = -ENOMEM;

	/* allocate memory for uname */
	v9ses->uname = get_free_page();
	if (!v9ses->uname)
		return ERR_PTR(-ENOMEM);

	/* allocate memory for aname */
	v9ses->aname = get_free_page();
	if (!v9ses->aname) {
		free_page(v9ses->uname);
		return ERR_PTR(-ENOMEM);
	}

	/* se uname/aname to default */
	strcpy(v9ses->uname, V9FS_DEFUSER);
	strcpy(v9ses->aname, V9FS_DEFANAME);

	/* create client */
	v9ses->client = p9_client_create(dev_name, data);
	if (IS_ERR(v9ses->client)) {
		ret = PTR_ERR(v9ses->client);
		v9ses->client = NULL;
		goto err;
	}

	/* parse options */
	ret = v9fs_parse_options(v9ses, data);
	if (ret)
		goto err;

	/* max data for client interface */
	v9ses->maxdata = v9ses->client->msize - P9_IOHDRSZ;

	/* attach to server */
	fid = p9_client_attach(v9ses->client, NULL, v9ses->uname, ~0, v9ses->aname);
	if (IS_ERR(fid)) {
		ret = PTR_ERR(fid);
		p9_error("v9fs_session_init: cannot attach\n");
		goto err;
	}

	return fid;
err:
	if (v9ses->client)
		p9_client_destroy(v9ses->client);
	free_page(v9ses->uname);
	free_page(v9ses->aname);
	return ERR_PTR(ret);
}

/*
 * Release a super block.
 */
static void v9fs_put_super(struct super_block *sb)
{
	UNUSED(sb);
	printf("TODO: v9fs_put_super\n");
}

/*
 * Get statistics on file system.
 */
static void v9fs_statfs(struct super_block *sb, struct statfs64 *buf)
{
	UNUSED(sb);
	UNUSED(buf);
	printf("TODO: v9fs_statfs\n");
}

/*
 * Superblock operations.
 */
static struct super_operations v9fs_sops = {
	.put_super		= v9fs_put_super,
	.read_inode		= v9fs_read_inode,
	.put_inode		= v9fs_put_inode,
	.statfs			= v9fs_statfs,
};

/*
 * Read super block.
 */
static struct super_block *v9fs_read_super(struct super_block *sb, const char *dev_name, void *data, int silent)
{
	struct v9fs_session_info *v9ses;
	struct inode *inode;
	struct p9_fid *fid;

	/* allocate a new session */
	sb->s_fs_info = v9ses = (struct v9fs_session_info *) kmalloc(sizeof(struct v9fs_session_info));
	if (!v9ses)
		return NULL;

	/* init session */
	fid = v9fs_session_init(v9ses, dev_name, data);
	if (IS_ERR(fid))
		goto err_session;

	/* set super block */
	sb->s_magic = V9FS_SUPER_MAGIC;
	sb->s_op = &v9fs_sops;

	/* get root inode */
	inode = v9fs_get_inode(sb, S_IFDIR | S_IRWXUGO);
	if (IS_ERR(inode))
		goto err_root_inode;

	printf("TODO: v9fs_read_super\n");
	return sb;
err_root_inode:
	if (!silent)
		p9_error("Can't get root inode\n");
	p9_client_clunk(fid);
	goto err;
err_session:
	if (!silent)
		p9_error("Can't init session\n");
err:
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
	int ret;

	/* init 9p */
	ret = init_p9();
	if (ret)
		return ret;

	/* register 9p filesystem */
	return register_filesystem(&v9fs_fs);
}
