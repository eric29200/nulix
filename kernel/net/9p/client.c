#include <fs/fs.h>
#include <net/9p/9p.h>
#include <lib/parser.h>
#include <stderr.h>

#define min(x, y)		((x) <= (y) ? (x) : (y))

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
 * Get next tag.
 */
uint16_t p9_client_next_tag(struct p9_client *client)
{
	uint16_t ret = client->tag++;

	if (client->tag == USHRT_MAX)
		client->tag++;

	return ret;
}


/*
 * Init a packet.
 */
static int p9_fcall_init(struct p9_fcall *fc, int alloc_msize)
{
	/* allocate data */
	fc->sdata = kmalloc(alloc_msize);
	if (!fc->sdata)
		return -ENOMEM;

	/* set packet */
	fc->capacity = alloc_msize;
	fc->id = 0;
	fc->tag = P9_NOTAG;

	return 0;
}

/*
 * Free a packet.
 */
void p9_fcall_fini(struct p9_fcall *fc)
{
	if (fc->sdata)
		kfree(fc->sdata);
}

/*
 * Free a request.
 */
void p9_request_free(struct p9_request *req)
{
	p9_fcall_fini(&req->tc);
	p9_fcall_fini(&req->rc);
	kfree(req);
}

/*
 * Allocate a new request.
 */
static struct p9_request *p9_tag_alloc(struct p9_client *client, int8_t type, const char *fmt, va_list ap)
{
	int alloc_tsize, alloc_rsize;
	struct p9_request *req;
	va_list apc;

	/* allocate a new request */
	req = (struct p9_request *) kmalloc(sizeof(struct p9_request));
	if (!req)
		return ERR_PTR(-ENOMEM);

	/* get transmit packet size */
	va_copy(apc, ap);
	alloc_tsize = min(client->msize, p9_msg_buf_size(type, fmt, apc));
	va_end(apc);

	/* get reply packet size */
	alloc_rsize = min(client->msize, p9_msg_buf_size(type + 1, fmt, ap));

	/* init packets */
	if (p9_fcall_init(&req->tc, alloc_tsize))
		goto err1;
	if (p9_fcall_init(&req->rc, alloc_rsize))
		goto err2;

	/* reset packets */
	p9pdu_reset(&req->tc);
	p9pdu_reset(&req->rc);

	/* get a tag */
	req->tc.tag = type == P9_TVERSION ? P9_NOTAG : p9_client_next_tag(client);

	return req;
err2:
	p9_fcall_fini(&req->tc);
	p9_fcall_fini(&req->rc);
err1:
	kfree(req);
	return ERR_PTR(-ENOMEM);
}

/*
 * Prepare a request.
 */
static struct p9_request *p9_client_prepare_req(struct p9_client *client, int8_t type, const char *fmt, va_list ap)
{
	struct p9_request *req;
	va_list apc;
	int ret;

	/* client disconnected */
	if (client->status == P9_CLIENT_DISCONNECTED)
		return ERR_PTR(-EIO);

	/* allocate request */
	va_copy(apc, ap);
	req = p9_tag_alloc(client, type, fmt, apc);
	va_end(apc);
	if (IS_ERR(req))
		return req;

	/* marshall data */
	p9pdu_prepare(&req->tc, req->tc.tag, type);
	ret = p9pdu_vwritef(&req->tc, fmt, ap);
	if (ret)
		goto err;

	/* finalize request */
	p9pdu_finalize(&req->tc);

	return req;
err:
	p9_request_free(req);
	return ERR_PTR(ret);
}

/*
 * Parse a 9p packet header.
 */
int p9_parse_header(struct p9_fcall *fc, int32_t *size, int8_t *type, int16_t *tag)
{
	int32_t r_size;
	int16_t r_tag;
	int8_t r_type;
	int ret;

	/* rewind to header */
	fc->offset = 0;

	/* read packet size, type and tag */
	ret = p9pdu_readf(fc, "dbw", &r_size, &r_type, &r_tag);
	if (ret)
		return ret;

	/* set output values */
	if (type)
		*type = r_type;
	if (tag)
		*tag = r_tag;
	if (size)
		*size = r_size;

	/* check packet size */
	if (fc->size != (uint32_t) r_size || r_size < 7)
		return -EINVAL;

	/* set packet */
	fc->id = r_type;
	fc->tag = r_tag;

	return 0;
}

/*
 * Check errors on a request.
 */
static int p9_check_errors(struct p9_request *req)
{
	int ret, error_code;
	int8_t type;

	/* parse reply header */
	ret = p9_parse_header(&req->rc, NULL, &type, NULL);
	if (ret) {
		p9_error("couldn't parse header\n");
		return ret;
	}

	/* check packet size */
	if (req->rc.size > req->rc.capacity) {
		p9_error("requested packet size too big: %d does not fit %zu (type=%d)\n", req->rc.size, req->rc.capacity, req->rc.id);
		return -EIO;
	}

	/* ok */
	if (type != P9_RERROR && type != P9_RLERROR)
		return 0;

	/* read error code */
	ret = p9pdu_readf(&req->rc, "d", &error_code);
	if (ret) {
		p9_error("couldn't parse error code\n");
		return ret;
	}

	/* print error code */
	p9_debug("RLERROR (%d)\n", error_code);

	return -error_code;
}

/*
 * Issue a request and wait for a response.
 */
static struct p9_request *p9_client_rpc(struct p9_client *client, int8_t type, const char *fmt, ...)
{
	struct p9_request *req;
	va_list ap;
	int ret;

	/* client disconnected */
	if (client->status == P9_CLIENT_DISCONNECTED)
		return ERR_PTR(-EIO);

	/* prepare request */
	va_start(ap, fmt);
	req = p9_client_prepare_req(client, type, fmt, ap);
	va_end(ap);
	if (IS_ERR(req))
		return req;

	/* issue request */
	ret = client->trans_mod->request(client, req);
	if (ret < 0)
		goto err;

	/* check errors */
	ret = p9_check_errors(req);
	if (ret)
		goto err;

	return req;
err:
	p9_request_free(req);
	return ERR_PTR(ret);
}

/*
 * Version request.
 */
int p9_client_version(struct p9_client *client)
{
	struct p9_request *req;
	char *version = NULL;
	int ret, msize;

	/* print a debug message */
	p9_debug("TVERSION msize %d protocol %d\n", client->msize, client->proto_version);

	/* send request */
	switch (client->proto_version) {
		case P9_PROTO_2000L:
			req = p9_client_rpc(client, P9_TVERSION, "ds", client->msize, "9P2000.L");
			break;
		default:
			return -EINVAL;
	}

	/* error on request */
	if (IS_ERR(req))
		return PTR_ERR(req);

	/* read reply */
	ret = p9pdu_readf(&req->rc, "ds", &msize, &version);
	if (ret)
		goto out;

	/* print reply */
	p9_debug("RVERSION msize %d %s\n", msize, version);

	/* set version */
	if (strncmp(version, "9P2000.L", 8) == 0) {
		client->proto_version = P9_PROTO_2000L;
	} else {
		p9_error("Server returned an unknown version: %s\n", version);
		ret = -EREMOTEIO;
		goto out;
	}

	/* check message size */
	if (msize < 4096) {
		p9_error("Server returned a msize < 4096: %d\n", msize);
		ret = -EREMOTEIO;
		goto out;
	}

	/* set message size */
	if (msize < client->msize)
		client->msize = msize;

out:
	if (version)
		kfree(version);
	p9_request_free(req);
	return ret;
}