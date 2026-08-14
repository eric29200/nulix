#include <fs/fs.h>
#include <net/9p/9p.h>
#include <lib/parser.h>
#include <proc/sched.h>
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
 * Create a fid.
 */
static struct p9_fid *p9_fid_create(struct p9_client *client)
{
	struct p9_fid *fid;

	/* allocate a new fid */
	fid = (struct p9_fid *) kmalloc(sizeof(struct p9_fid));
	if (!fid)
		return NULL;

	/* init fid */
	memset(fid, 0, sizeof(struct p9_fid));
	fid->mode = -1;
	fid->uid = current_task->fsuid;
	fid->client = client;
	fid->fid = client->fid++;
	list_add(&fid->flist, &client->fid_list);
	if (client->fid == UINT_MAX)
		client->fid++;

	return fid;
}

/*
 * Destroy a fid.
 */
static void p9_fid_destroy(struct p9_fid *fid)
{
	list_del(&fid->flist);
	if (fid->rdir)
		kfree(fid->rdir);
	kfree(fid);
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
 * Destroy a client.
 */
void p9_client_destroy(struct p9_client *client)
{
	struct list_head *pos, *n;
	struct p9_fid *fid;

	/* close connection */
	if (client->trans_mod)
		client->trans_mod->close(client);

	/* destroy fids */
	list_for_each_safe(pos, n, &client->fid_list) {
		fid = list_entry(pos, struct p9_fid, flist);
		p9_fid_destroy(fid);
	}

	/* free client */
	kfree(client);
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
	uint32_t msize;
	int ret;

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

/*
 * Attach request.
 */
struct p9_fid *p9_client_attach(struct p9_client *client, struct p9_fid *afid, const char *uname, uid_t n_uname, const char *aname)
{
	struct p9_request *req;
	struct p9_fid *fid;
	struct p9_qid qid;
	int ret;

	/* print a debug message */
	p9_debug("TATTACH afid %d uname %s aname %s\n", afid ? (int) afid->fid : -1, uname, aname);

	/* create file identifier */
	fid = p9_fid_create(client);
	if (!fid)
		return ERR_PTR(-ENOMEM);
	fid->uid = n_uname;

	/* issue attach request */
	req = p9_client_rpc(client, P9_TATTACH, "ddssu", fid->fid, afid ? afid->fid : P9_NOFID, uname, aname, n_uname);
	if (IS_ERR(req)) {
		ret = PTR_ERR(req);
		goto err;
	}

	/* read reply */
	ret = p9pdu_readf(&req->rc, "Q", &qid);
	if (ret)
		goto err_free_req;

	/* print reply */
	p9_debug("RATTACH qid %x.%llx.%x\n", qid.type, qid.path, qid.version);

	/* set file identifier from server */
	memcpy(&fid->qid, &qid, sizeof(struct p9_qid));

	/* free request */
	p9_request_free(req);

	return fid;
err_free_req:
	p9_request_free(req);
err:
	p9_fid_destroy(fid);
	return ERR_PTR(ret);
}

/*
 * Release a fid.
 */
int p9_client_clunk(struct p9_fid *fid)
{
	struct p9_client *client = fid->client;
	struct p9_request *req;
	int ret = 0;

	/* print a debug message */
	p9_debug("TCLUNK fid %d\n", fid->fid);

	/* issue request */
	req = p9_client_rpc(client, P9_TCLUNK, "d", fid->fid);
	if (IS_ERR(req)) {
		ret = PTR_ERR(req);
		goto out;
	}

	/* print reply */
	p9_debug("RCLUNK fid %d\n", fid->fid);

	/* free request */
	p9_request_free(req);
out:
	p9_fid_destroy(fid);
	return ret;
}

/*
 * Get attributes.
 */
struct p9_stat *p9_client_getattr(struct p9_fid *fid, uint64_t request_mask)
{
	struct p9_client *client = fid->client;
	struct p9_request *req;
	struct p9_stat *st;
	int ret;

	/* print a debug message */
	p9_debug("TGETATTR fid %d, request_mask %lld\n", fid->fid, request_mask);

	/* allocate a stat buffer */
	st = (struct p9_stat *) kmalloc(sizeof(struct p9_stat));
	if (!st)
		return ERR_PTR(-ENOMEM);

	/* issue request */
	req = p9_client_rpc(client, P9_TGETATTR, "dq", fid->fid, request_mask);
	if (IS_ERR(req)) {
		ret = PTR_ERR(req);
		goto err;
	}

	/* read reply */
	ret = p9pdu_readf(&req->rc, "A", st);
	if (ret) {
		p9_request_free(req);
		goto err;
	}

	/* print a debug message */
	p9_debug("RGETATTR st_result_mask=%lld qid=%x.%llx.%x\n",
		 st->st_result_mask,
		 st->qid.type,
		 st->qid.path,
		 st->qid.version);

	/* free request */
	p9_request_free(req);

	return st;
err:
	kfree(st);
	return ERR_PTR(ret);
}

/*
 * Open a file.
 */
int p9_client_open(struct p9_fid *fid, int mode)
{
	struct p9_client *client = fid->client;
	struct p9_request *req;
	struct p9_qid qid;
	int ret, iounit;

	/* print a debug message */
	p9_debug("TLOPEN fid %d mode %d\n", fid->fid, mode);

	/* already opened ? */
	if (fid->mode != -1)
		return -EINVAL;

	/* issue request */
	req = p9_client_rpc(client, P9_TLOPEN, "dd", fid->fid, mode & P9_MODE_MASK);
	if (IS_ERR(req))
		return PTR_ERR(req);

	/* read reply */
	ret = p9pdu_readf(&req->rc, "Qd", &qid, &iounit);
	if (ret)
		goto out;

	/* print reply */
	p9_debug("RLOPEN qid %x.%llx.%x iounit %x\n", qid.type, qid.path, qid.version, iounit);

	/* update fid */
	memcpy(&fid->qid, &qid, sizeof(struct p9_qid));
	fid->mode = mode;
	fid->iounit = iounit;
out:
	p9_request_free(req);
	return ret;
}

/*
 * Walk a path.
 */
struct p9_fid *p9_client_walk(struct p9_fid *oldfid, uint16_t nwname, char **wnames, int clone)
{
	struct p9_client *client = oldfid->client;
	struct p9_fid *fid = oldfid;
	struct p9_request *req;
	struct p9_qid *wqids;
	uint16_t nwqids;
	int ret;

	/* clone fid ? */
	if (clone) {
		fid = p9_fid_create(client);
		if (IS_ERR(fid))
			return fid;
		fid->uid = oldfid->uid;
	} else {
		fid = oldfid;
	}

	/* print a debug message */
	p9_debug("TWALK fids %d,%d nwname %ud wname[0] %s\n", oldfid->fid, fid->fid, nwname, wnames ? wnames[0] : NULL);

	/* issue request */
	req = p9_client_rpc(client, P9_TWALK, "ddT", oldfid->fid, fid->fid, nwname, wnames);
	if (IS_ERR(req)) {
		ret = PTR_ERR(req);
		goto err;
	}

	/* read reply */
	ret = p9pdu_readf(&req->rc, "R", &nwqids, &wqids);
	if (ret) {
		p9_request_free(req);
		goto err_clunk_fid;
	}

	/* print reply */
	p9_debug("RWALK nwqid %d\n", nwqids);

	/* free request */
	p9_request_free(req);

	/* check reply */
	if (nwqids != nwname) {
		ret = -ENOENT;
		goto err_clunk_fid;
	}

	/* set names/qid */
	if (nwname)
		memcpy(&fid->qid, &wqids[nwqids - 1], sizeof(struct p9_qid));
	else
		fid->qid = oldfid->qid;

	/* free qids*/
	kfree(wqids);

	return fid;
err_clunk_fid:
	kfree(wqids);
	p9_client_clunk(fid);
	fid = NULL;
err:
	if (fid && fid != oldfid)
		p9_fid_destroy(fid);
	return ERR_PTR(ret);
}

/*
 * Read a directory.
 */
int p9_client_readdir(struct p9_fid *fid, char *buf, uint32_t count, uint64_t offset)
{
	struct p9_client *client = fid->client;
	struct p9_request *req;
	uint32_t rsize;
	char *dataptr;
	int ret;

	/* print a debug message */
	p9_debug("TREADDIR fid %d offset %llu count %u\n", fid->fid, offset, count);

	/* choose read size */
	rsize = fid->iounit;
	if (!rsize || rsize > client->msize - P9_READDIRHDRSZ)
		rsize = client->msize - P9_READDIRHDRSZ;

	/* limit read size */
	if (count < rsize)
		rsize = count;

	/* issue request */
	req = p9_client_rpc(client, P9_TREADDIR, "dqd", fid->fid, offset, rsize);
	if (IS_ERR(req))
		return PTR_ERR(req);

	/* read reply */
	ret = p9pdu_readf(&req->rc, "D", &count, &dataptr);
	if (ret)
		goto err;

	/* check read size */
	if (rsize < count) {
		p9_error("bogus RREADDIR count (%u > %u)\n", count, rsize);
		ret = -EIO;
		goto err;
	}

	/* print reply */
	p9_debug("RREADDIR count %u\n", count);

	/* copy data */
	memcpy(buf, dataptr, count);

	/* free request */
	p9_request_free(req);

	return count;
err:
	p9_request_free(req);
	return ret;
}

/*
 * Read request.
 */
int p9_client_read(struct p9_fid *fid, char *buf, uint64_t offset, uint32_t count)
{
	struct p9_client *client = fid->client;
	uint32_t r_size, received;
	struct p9_request *req;
	char *dataptr;
	int ret;

	/* print a debug message */
	p9_debug("TREAD fid %d offset %llu %d\n", fid->fid, offset, count);

	/* choose read size */
	r_size = fid->iounit;
	if (!r_size || r_size > client->msize - P9_IOHDRSZ)
		r_size = client->msize - P9_IOHDRSZ;

	/* limit read size */
	if (count < r_size)
		r_size = count;

	/* issue request */
	req = p9_client_rpc(client, P9_TREAD, "dqd", fid->fid, offset, r_size);
	if (IS_ERR(req))
		return PTR_ERR(req);

	/* read reply */
	ret = p9pdu_readf(&req->rc, "D", &received, &dataptr);
	if (ret)
		goto err;

	/* check received */
	if (r_size < received) {
		p9_error("bogus RREAD count (%u > %u)\n", received, r_size);
		ret = -EIO;
		goto err;
	}

	/* print reply */
	p9_debug("RREAD count %u\n", received);

	/* get data */
	memcpy(buf, dataptr, received);

	/* free request */
	p9_request_free(req);

	return received;
err:
	p9_request_free(req);
	return ret;
}

/*
 * File system statistics request.
 */
int p9_client_statfs(struct p9_fid *fid, struct p9_rstatfs *st)
{
	struct p9_client *client = fid->client;
	struct p9_request *req;
	int ret;

	/* print a debug message */
	p9_debug("TSTATFS fid %d\n", fid->fid);

	/* issue request */
	req = p9_client_rpc(client, P9_TSTATFS, "d", fid->fid);
	if (IS_ERR(req))
		return PTR_ERR(req);

	/* read reply */
	ret = p9pdu_readf(&req->rc, "ddqqqqqqd",
		&st->type,
		&st->bsize,
		&st->blocks,
		&st->bfree,
		&st->bavail,
		&st->files,
		&st->ffree,
		&st->fsid,
		&st->namelen);
	if (ret)
		goto out;

	/* print reply */
	p9_debug("RSTATFS fid %d type 0x%x ...\n", fid->fid, st->type);

out:
	p9_request_free(req);
	return ret;
}

/*
 * Read link request.
 */
int p9_client_readlink(struct p9_fid *fid, char **target)
{
	struct p9_client *client = fid->client;
	struct p9_request *req;
	int ret;

	/* print a debug message */
	p9_debug("TREADLINK fid %d\n", fid->fid);

	/* issue request */
	req = p9_client_rpc(client, P9_TREADLINK, "d", fid->fid);
	if (IS_ERR(req))
		return PTR_ERR(req);

	/* read reply */
	ret = p9pdu_readf(&req->rc, "s", target);
	if (ret)
		goto out;

	/* print a debug message */
	p9_debug("RREADLINK target %s\n", *target);
out:
	p9_request_free(req);
	return ret;
}

/*
 * Create a directory.
 */
int p9_client_mkdir(struct p9_fid *fid, const char *name, int mode, gid_t gid, struct p9_qid *qid)
{
	struct p9_client *client = fid->client;
	struct p9_request *req;
	int ret;

	/* print a debug message */
	p9_debug("TMKDIR fid %d name %s mode %d gid %d\n", fid->fid, name, mode, gid);

	/* issue request */
	req = p9_client_rpc(client, P9_TMKDIR, "dsdg", fid->fid, name, mode, gid);
	if (IS_ERR(req))
		return PTR_ERR(req);

	/* read reply */
	ret = p9pdu_readf(&req->rc, "Q", qid);
	if (ret)
		goto out;

	/* print reply */
	p9_debug("RMKDIR qid %x.%llx.%x\n", qid->type, qid->path, qid->version);
out:
	p9_request_free(req);
	return ret;
}

/*
 * Make a node.
 */
int p9_client_mknod(struct p9_fid *fid, const char *name, int mode, dev_t rdev, gid_t gid, struct p9_qid *qid)
{
	struct p9_client *client = fid->client;
	struct p9_request *req;
	int ret;

	/* print a debug message */
	p9_debug("TMKNOD fid %d name %s mode %d major %d minor %d\n", fid->fid, name, mode, major(rdev), minor(rdev));

	/* issue request */
	req = p9_client_rpc(client, P9_TMKNOD, "dsdddg", fid->fid, name, mode, major(rdev), minor(rdev), gid);
	if (IS_ERR(req))
		return PTR_ERR(req);

	/* read reply */
	ret = p9pdu_readf(&req->rc, "Q", qid);
	if (ret)
		goto out;

	/* print reply */
	p9_debug("RMKNOD qid %x.%llx.%x\n", qid->type, qid->path, qid->version);
out:
	p9_request_free(req);
	return ret;
}

/*
 * Create a link.
 */
int p9_client_link(struct p9_fid *dfid, struct p9_fid *oldfid, const char *newname)
{
	struct p9_client *client = dfid->client;
	struct p9_request *req;

	/* print a debug message */
	p9_debug("TLINK dfid %d oldfid %d newname %s\n", dfid->fid, oldfid->fid, newname);

	/* issue request */
	req = p9_client_rpc(client, P9_TLINK, "dds", dfid->fid, oldfid->fid, newname);
	if (IS_ERR(req))
		return PTR_ERR(req);

	/* print reply */
	p9_debug("RLINK\n");

	/* free request */
	p9_request_free(req);

	return 0;
}

/*
 * Create a symbolic link.
 */
int p9_client_symlink(struct p9_fid *dfid, const char *name, const char *symtgt, gid_t gid, struct p9_qid *qid)
{
	struct p9_client *client = dfid->client;
	struct p9_request *req;
	int ret;

	/* print a debug message */
	p9_debug("TSYMLINK dfid %d name %s  symtgt %s\n", dfid->fid, name, symtgt);

	/* issue request */
	req = p9_client_rpc(client, P9_TSYMLINK, "dssg", dfid->fid, name, symtgt, gid);
	if (IS_ERR(req))
		return PTR_ERR(req);

	/* read reply */
	ret = p9pdu_readf(&req->rc, "Q", qid);
	if (ret)
		goto out;

	/* print reply */
	p9_debug("RSYMLINK qid %x.%llx.%x\n", qid->type, qid->path, qid->version);
out:
	p9_request_free(req);
	return ret;
}

/*
 * Remove request.
 */
int p9_client_remove(struct p9_fid *fid)
{
	struct p9_client *client = fid->client;
	struct p9_request *req;

	/* print a debug message */
	p9_debug("TREMOVE fid %d\n", fid->fid);

	/* issue request */
	req = p9_client_rpc(client, P9_TREMOVE, "d", fid->fid);
	if (IS_ERR(req))
		return PTR_ERR(req);

	/* print reply */
	p9_debug("RREMOVE fid %d\n", fid->fid);

	/* free request */
	p9_request_free(req);

	/* destroy fid */
	p9_fid_destroy(fid);

	return 0;
}

/*
 * Rename a file.
 */
int p9_client_rename(struct p9_fid *fid, struct p9_fid *newdirfid, const char *name)
{
	struct p9_client *client = fid->client;
	struct p9_request *req;

	/* print a debug message */
	p9_debug("TRENAME fid %d newdirfid %d name %s\n", fid->fid, newdirfid->fid, name);

	/* issue request */
	req = p9_client_rpc(client, P9_TRENAME, "dds", fid->fid, newdirfid->fid, name);
	if (IS_ERR(req))
		return PTR_ERR(req);

	/* print reply */
	p9_debug("RRENAME fid %d\n", fid->fid);

	/* free request */
	p9_request_free(req);

	return 0;
}