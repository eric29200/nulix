#include <fs/fs.h>
#include <net/9p/9p.h>
#include <lib/parser.h>
#include <stderr.h>

/*
 * Options.
 */
enum {
	Opt_msize,
	Opt_trans,
	Opt_version,
	Opt_err,
};

static const struct match_token tokens[] = {
	{ Opt_msize,	"msize=%u"	},
	{ Opt_trans,	"trans=%s"	},
	{ Opt_version,	"version=%s"	},
	{ Opt_err,	NULL		},
};

/*
 * Get protocol version.
 */
static int get_protocol_version(const struct substring *name)
{
	if (strncmp("9p2000.L", name->from, name->to-name->from) == 0)
		return P9_PROTO_2000L;

	return -EINVAL;
}

/*
 * Parse client options.
 */
static int parse_opts(char *opts, struct p9_client *client)
{
	struct substring args[MAX_OPT_ARGS];
	char *options, *tmp_options, *p;
	int token, option, r, ret = 0;

	/* set defaults */
	client->proto_version = P9_PROTO_2000L;
	client->msize = 8192;

	/* no options */
	if (!opts)
		return 0;

	/* dup options */
	tmp_options = strdup(opts);
	if (!tmp_options) {
		p9_error("Failed to allocate copy of option string\n");
		return -ENOMEM;
	}

	/* parse options */
	options = tmp_options;
	for (;;) {
		/* get next option */
		p = strsep(&options, ",");
		if (!p)
			break;
		if (!*p)
			continue;

		/* parse int options */
		token = match_token(p, tokens, args);
		if (token < Opt_trans) {
			r = match_int(&args[0], &option);
			if (r < 0) {
				p9_error("Integer field, but no integer?\n");
				ret = r;
				continue;
			}
		}

		/* parse other options */
		switch (token) {
			case Opt_msize:
				client->msize = option;
				break;
			case Opt_trans:
				client->trans_mod = v9fs_get_trans_by_name(&args[0]);
				if (!client->trans_mod) {
					p9_error("Could not find request transport: %s\n", (char *) args[0].from);
					ret = -EINVAL;
					goto out;
				}
				break;
			case Opt_version:
				ret = get_protocol_version(&args[0]);
				if (ret == -EINVAL)
					goto out;
				client->proto_version = ret;
				break;
			default:
				continue;
		}
	}

out:
	kfree(tmp_options);
	return ret;
}

/*
 * Create a client.
 */
struct p9_client *p9_client_create(const char *dev_name, char *options)
{
	struct p9_client *client;
	int ret;

	/* allocate client */
	client = kmalloc(sizeof(struct p9_client));
	if (!client)
		return ERR_PTR(-ENOMEM);

	/* init client */
	memset(client, 0, sizeof(struct p9_client));
	INIT_LIST_HEAD(&client->fid_list);

	/* parse options */
	ret = parse_opts(options, client);
	if (ret < 0)
		goto err;

	/* get default transport module */
	if (!client->trans_mod)
		client->trans_mod = v9fs_get_default_trans();

	/* no transport module */
	if (!client->trans_mod) {
		ret = -EPROTONOSUPPORT;
		p9_error("No transport defined or default transport\n");
		goto err;
	}

	/* transport create */
	ret = client->trans_mod->create(client, dev_name, options);
	if (ret)
		goto err;

	/* limit message size */
	if (client->msize + P9_IOHDRSZ > client->trans_mod->maxsize)
		client->msize = client->trans_mod->maxsize - P9_IOHDRSZ;

	/* issue version request */
	ret = p9_client_version(client);
	if (ret)
		goto err_close_trans;

	return client;
err_close_trans:
	client->trans_mod->close(client);
err:
	kfree(client);
	return ERR_PTR(ret);
}

/*
 * Version request.
 */
int p9_client_version(struct p9_client *client)
{
	UNUSED(client);
	printf("TODO: p9_client_version\n");
	return -EINVAL;
}