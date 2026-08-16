#include <net/9p/9p.h>
#include <net/socket.h>
#include <net/inet/ip.h>
#include <net/inet/in.h>
#include <net/inet/net.h>
#include <x86/endian.h>
#include <proc/sched.h>
#include <proc/tqueue.h>
#include <fs/fs.h>
#include <stdio.h>
#include <stderr.h>
#include <mm/mm.h>
#include <string.h>
#include <fcntl.h>

#define MAX_SOCK_BUF		(64 * 1024)

#define RWORKSCHED		1	/* read work scheduled */
#define RPENDING		2	/* can read */
#define WWORKSCHED		4	/* write work scheduled */
#define WPENDING		8	/* can write */

/* connections */
static LIST_HEAD(p9_poll_pending_list);
static struct tqueue poll_tq;

/*
 * 9p poll table.
 */
struct p9_poll_wait {
	struct p9_conn *		conn;
	struct wait_queue		wait;
	struct wait_queue_head *	wait_addr;
};

/*
 * Connection.
 */
struct p9_conn {
	struct p9_client *	client;
	int			err;
	struct list_head	req_list;
	struct list_head	unsent_req_list;
	struct p9_request *	req;
	char			tmp_buf[7];
	int			rsize;
	int			rpos;
	char *			rbuf;
	int			wsize;
	int			wpos;
	char *			wbuf;
	struct p9_poll_wait	poll_wait;
	struct poll_table	pt;
	uint32_t		wsched;
	struct list_head	poll_pending_link;
	struct tqueue		rq;
	struct tqueue		wq;
};

/*
 * File transport.
 */
struct p9_trans_fd {
	struct file *		filp;
	struct p9_conn *	conn;
};

/*
 * Options.
 */
enum {
	Opt_port,
	Opt_err,
};

static const struct match_token tokens[] = {
	{ Opt_port,	"port=%u"	},
	{ Opt_err,	NULL		},
};

/*
 * File transport options.
 */
struct p9_fd_opts {
	uint16_t	port;
};

static void p9_conn_cancel(struct p9_conn *conn, int err);

/*
 * Parse options.
 */
static int parse_opts(char *params, struct p9_fd_opts *opts)
{
	struct substring args[MAX_OPT_ARGS];
	char *options, *tmp_options, *p;
	int token, option, r;

	/* defualt options */
	opts->port = P9_PORT;

	/* no options */
	if (!params)
		return 0;

	/* dup options */
	tmp_options = strdup(params);
	if (!tmp_options)
		return -ENOMEM;
	options = tmp_options;

	for (;;) {
		/* get next option */
		p = strsep(&options, ",");
		if (!p)
			break;
		if (!*p)
			continue;
		if (!*p)
			continue;

		/* parse int option */
		token = match_token(p, tokens, args);
		if (token != Opt_err) {
			r = match_int(&args[0], &option);
			if (r < 0) {
				p9_error("Integer field, but no integer?\n");
				return r;
			}
		}

		/* parse option */
		switch (token) {
			case Opt_port:
				opts->port = option;
				break;
			default:
				continue;
		}
	}

	kfree(tmp_options);
	return 0;
}

/*
 * Find a request by tag.
 */
static struct p9_request *p9_tag_lookup(struct p9_client *client, uint16_t tag)
{
	struct p9_trans_fd *trans = client->trans;
	struct p9_conn *conn = trans->conn;
	struct p9_request *req;
	struct list_head *pos;

	list_for_each(pos, &conn->req_list) {
		req = list_entry(pos, struct p9_request, list);
		if (req->tc.tag == tag)
			return req;
	}

	return NULL;
}

/*
 * Poll connection.
 */
static int p9_fd_poll(struct p9_client *client, struct poll_table *pt)
{
	struct p9_trans_fd *ts = NULL;

	/* check if client is connected */
	if (client && client->status == P9_CLIENT_CONNECTED)
		ts = client->trans;
	if (!ts)
		return -EREMOTEIO;

	/* poll not implemented */
	if (!ts->filp->f_op || !ts->filp->f_op->poll)
		return -EIO;

	/* poll */
	return ts->filp->f_op->poll(ts->filp, pt);
}

/*
 * Read from a socket.
 */
static int p9_fd_read(struct p9_client *client, void *buf, int len)
{
	struct p9_trans_fd *trans = NULL;
	int ret;

	/* client must be connected */
	if (client && client->status != P9_CLIENT_DISCONNECTED)
		trans = client->trans;
	if (!trans)
		return -EREMOTEIO;

	/* check permssions */
	if (!(trans->filp->f_mode & FMODE_READ))
		return -EBADF;
	if (!trans->filp->f_op || !trans->filp->f_op->read)
		return -EINVAL;

	/* write */
	ret = trans->filp->f_op->read(trans->filp, buf, len, &trans->filp->f_pos);
	if (ret <= 0 && ret != -ERESTARTSYS && ret != -EAGAIN)
		client->status = P9_CLIENT_DISCONNECTED;

	return ret;
}

/*
 * Read work = receive requests.
 */
static void p9_read_work(void *arg)
{
	struct p9_conn *conn = (struct p9_conn *) arg;
	uint16_t tag;
	uint32_t n;
	int ret;

	/* connection error */
	if (conn->err < 0)
		return;

	/* start by reading header */
	if (!conn->rbuf) {
		conn->rbuf = conn->tmp_buf;
		conn->rpos = 0;
		conn->rsize = P9_HDRSZ;
	}

	/* read request */
	clear_bit(&conn->wsched, RPENDING);
	ret = p9_fd_read(conn->client, conn->rbuf + conn->rpos, conn->rsize - conn->rpos);
	if (ret == -EAGAIN) {
		clear_bit(&conn->wsched, RWORKSCHED);
		return;
	}
	if (ret <= 0)
		goto err;

	/* update read position */
	conn->rpos += ret;

	/* read header */
	if (!conn->req && conn->rpos == conn->rsize) {
		/* read packet size */
		n = le32toh(*((uint32_t *) conn->rbuf));
		if (n >= conn->client->msize) {
			ret = -EIO;
			goto err;
		}

		/* read tag */
		tag = le16toh(*((uint16_t *) (conn->rbuf + 5)));

		/* find request */
		conn->req = p9_tag_lookup(conn->client, tag);
		if (!conn->req || (conn->req->status != P9_REQUEST_STATUS_SENT && conn->req->status != P9_REQUEST_STATUS_FLSH)) {
			ret = -EIO;
			goto err;
		}

		/* get data */
		conn->rbuf = (char *) conn->req->rc.sdata;
		memcpy(conn->rbuf, conn->tmp_buf, conn->rsize);
		conn->rsize = n;
	}

	/* packet is read in */
	if (conn->req && conn->rpos == conn->rsize) {
		if (conn->req->status != P9_REQUEST_STATUS_ERROR)
			conn->req->status = P9_REQUEST_STATUS_RCVD;
		list_del(&conn->req->list);
		p9_client_cb(conn->req);
		conn->req->rc.size = conn->rsize;
		conn->rbuf = NULL;
		conn->rpos = 0;
		conn->rsize = 0;
		conn->req = NULL;
	}

	/* reschedule read if needed */
	if (!list_empty(&conn->req_list)) {
		if (test_and_clear_bit(&conn->wsched, RPENDING))
			n = POLLIN;
		else
			n = p9_fd_poll(conn->client, NULL);

		if (n & POLLIN)
		 	queue_task(&conn->rq);
		else
			clear_bit(&conn->wsched, RWORKSCHED);
	} else {
		clear_bit(&conn->wsched, RWORKSCHED);
	}

	return;
err:
	p9_conn_cancel(conn, ret);
	clear_bit(&conn->wsched, RWORKSCHED);
}

/*
 * Write to a socket.
 */
static int p9_fd_write(struct p9_client *client, void *buf, int len)
{
	struct p9_trans_fd *trans = NULL;
	int ret;

	/* client must be connected */
	if (client && client->status != P9_CLIENT_DISCONNECTED)
		trans = client->trans;
	if (!trans)
		return -EREMOTEIO;

	/* check permssions */
	if (!(trans->filp->f_mode & FMODE_WRITE))
		return -EBADF;
	if (!trans->filp->f_op || !trans->filp->f_op->write)
		return -EINVAL;

	/* write */
	ret = trans->filp->f_op->write(trans->filp, buf, len, &trans->filp->f_pos);
	if (ret <= 0 && ret != -ERESTARTSYS && ret != -EAGAIN)
		client->status = P9_CLIENT_DISCONNECTED;

	return ret;
}

/*
 * Write work = send requests.
 */
static void p9_write_work(void *arg)
{
	struct p9_conn *conn = (struct p9_conn *) arg;
	struct p9_request *req;
	int ret, n;

	/* error on connection */
	if (conn->err < 0) {
		clear_bit(&conn->wsched, WWORKSCHED);
		return;
	}

	/* no request being sent : choose a new one */
	if (!conn->wsize) {
		/* no more request */
		if (list_empty(&conn->unsent_req_list)) {
			clear_bit(&conn->wsched, WWORKSCHED);
			return;
		}

		/* pick first request */
		req = list_first_entry(&conn->unsent_req_list, struct p9_request, list);
		req->status = P9_REQUEST_STATUS_SENT;
		conn->wbuf = (char *) req->tc.sdata;
		conn->wsize = req->tc.size;
		conn->wpos = 0;

		/* move request */
		list_del(&req->list);
		list_add_tail(&req->list, &conn->req_list);
	}

	/* write request */
	clear_bit(&conn->wsched, WPENDING);
	ret = p9_fd_write(conn->client, conn->wbuf + conn->wpos, conn->wsize - conn->wpos);
	if (ret == -EAGAIN) {
		clear_bit(&conn->wsched, WWORKSCHED);
		return;
	}

	/* handle error */
	if (ret == 0)
		ret = -EREMOTEIO;
	if (ret < 0)
		goto err;

	/* update write position */
	conn->wpos += ret;
	if (conn->wpos == conn->wsize)
		conn->wpos = conn->wsize = 0;

	/* schedule next request */
	if (conn->wsize == 0 && !list_empty(&conn->unsent_req_list)) {
		if (test_and_clear_bit(&conn->wsched, WPENDING))
			n = POLLOUT;
		else
			n = p9_fd_poll(conn->client, NULL);

		if (n & POLLOUT)
			queue_task(&conn->wq);
		else
			clear_bit(&conn->wsched, WWORKSCHED);
	} else {
		clear_bit(&conn->wsched, WWORKSCHED);
	}

	return;
err:
	p9_conn_cancel(conn, ret);
	clear_bit(&conn->wsched, WWORKSCHED);
}

/**
 * Polls a connection and schedules read or write works if necessary.
 */
static void p9_poll_mux(struct p9_conn *conn)
{
	int n;

	/* connection error */
	if (conn->err < 0)
		return;

	/* poll */
	n = p9_fd_poll(conn->client, NULL);
	if (n < 0 || n & (POLLERR | POLLHUP | POLLNVAL)) {
		if (n >= 0)
			n = -ECONNRESET;
		p9_conn_cancel(conn, n);
	}

	/* handle read */
	if (n & POLLIN) {
		set_bit(&conn->wsched, RPENDING);
		if (!test_and_set_bit(&conn->wsched, RWORKSCHED))
			queue_task(&conn->rq);
	}

	/* handle write */
	if (n & POLLOUT) {
		set_bit(&conn->wsched, WPENDING);
		if ((conn->wsize || !list_empty(&conn->unsent_req_list)) && !test_and_set_bit(&conn->wsched, WWORKSCHED))
			queue_task(&conn->wq);
	}
}

/*
 * Stop polling a connection.
 */
static void p9_mux_poll_stop(struct p9_conn *conn)
{
	struct p9_poll_wait *pwait = &conn->poll_wait;

	if (pwait->wait_addr) {
		remove_wait_queue(&pwait->wait);
		pwait->wait_addr = NULL;
	}

	list_del(&conn->poll_pending_link);
}

/*
 * Poll pending connections.
 */
static void p9_poll_work(void *arg)
{
	struct list_head *pos;
	struct p9_conn *conn;

	/* no argument */
	UNUSED(arg);

	/* poll connections */
	list_for_each(pos, &p9_poll_pending_list) {
		conn = list_entry(pos, struct p9_conn, poll_pending_link);
		p9_poll_mux(conn);
	}
}

/*
 * Poll callback.
 */
static int p9_pollwake(struct wait_queue *wait)
{
	struct p9_poll_wait *pwait = container_of(wait, struct p9_poll_wait, wait);
	struct p9_conn *conn = pwait->conn;

	/* add current connection if needed */
	if (list_empty(&conn->poll_pending_link))
		list_add_tail(&conn->poll_pending_link, &p9_poll_pending_list);

	/* queue poll multiplexer */
	queue_task(&poll_tq);

	return 1;
}

/*
 * Add wait queue to poll table.
 */
static void p9_pollwait(struct wait_queue_head *wait_address, struct poll_table *pt)
{
	struct p9_conn *conn = container_of(pt, struct p9_conn, pt);
	struct p9_poll_wait *pwait = &conn->poll_wait;

	/* check if wait adress is free */
	if (pwait->wait_addr) {
		p9_error("p9_pollwait: not enouth wait_address slots\n");
		return;
	}

	/* set poll table */
	pwait->conn = conn;
	pwait->wait_addr = wait_address;
	init_waitqueue_func_entry(&pwait->wait, p9_pollwake);
	add_wait_queue(wait_address, &pwait->wait);
}

/*
 * Create a connection.
 */
static struct p9_conn *p9_conn_create(struct p9_client *client)
{
	struct p9_conn *conn;
	int n;

	/* allocate connection */
	conn = (struct p9_conn *) kmalloc(sizeof(struct p9_conn));
	if (!conn)
		return ERR_PTR(-ENOMEM);

	/* init connection */
	memset(conn, 0, sizeof(struct p9_conn));
	conn->client = client;
	INIT_LIST_HEAD(&conn->req_list);
	INIT_LIST_HEAD(&conn->unsent_req_list);
	INIT_TQUEUE(&conn->rq, &p9_read_work, conn);
	INIT_TQUEUE(&conn->wq, &p9_write_work, conn);
	list_add_tail(&conn->poll_pending_link, &p9_poll_pending_list);
	init_poll_funcptr(&conn->pt, p9_pollwait);

	/* poll client */
	n = p9_fd_poll(client, &conn->pt);
	if (n & POLLIN)
		set_bit(&conn->wsched, RPENDING);
	if (n & POLLOUT)
		set_bit(&conn->wsched, WPENDING);

	return conn;
}

/*
 * Destroy a connection.
 */
static void p9_conn_destroy(struct p9_conn *conn)
{
	/* stop task queues */
	p9_mux_poll_stop(conn);
	unqueue_task(&conn->rq);
	unqueue_task(&conn->wq);

	/* cancel connection */
	p9_conn_cancel(conn, -ECONNRESET);

	/* free connection */
	conn->client = NULL;
	kfree(conn);
}

/*
 * Cancel a connection.
 */
static void p9_conn_cancel(struct p9_conn *conn, int err)
{
	struct list_head *pos, *n;
	struct p9_request *req;
	LIST_HEAD(cancel_list);

	/* print a debug message */
	p9_error("mux %x err %d\n", conn, err);

	/* set error */
	if (conn->err)
		return;
	conn->err = err;

	/* move requests to cancel list */
	list_for_each_safe(pos, n, &conn->req_list) {
		req = list_entry(pos, struct p9_request, list);
		req->status = P9_REQUEST_STATUS_ERROR;
		if (!req->t_err)
			req->t_err = err;
		list_move(&req->list, &cancel_list);
	}

	/* move unsent requests to cancel list */
	list_for_each_safe(pos, n, &conn->unsent_req_list) {
		req = list_entry(pos, struct p9_request, list);
		req->status = P9_REQUEST_STATUS_ERROR;
		if (!req->t_err)
			req->t_err = err;
		list_move(&req->list, &cancel_list);
	}

	/* cancel requests */
	list_for_each_safe(pos, n, &conn->req_list) {
		req = list_entry(pos, struct p9_request, list);
		list_del(&req->list);
		wake_up(&req->wait);
	}
}

/*
 * Attach socket to the client.
 */
static int p9_socket_open(struct p9_client *client, struct socket *sock)
{
	struct p9_trans_fd *trans;
	struct file *filp;
	int ret;

	/* allocate file transport */
	trans = (struct p9_trans_fd *) kmalloc(sizeof(struct p9_trans_fd));
	if (!trans)
		return -ENOMEM;

	/* get a file */
	filp = sock_get_file(sock);
	if (IS_ERR(filp)) {
		p9_error("p9_socket_open: failed to map fd\n");
		sock_release(sock);
		kfree(trans);
		return PTR_ERR(filp);
	}

	/* install file */
	filp->f_flags = O_NONBLOCK;
	trans->filp = filp;
	sock->file = filp;
	client->trans = trans;
	client->status = P9_CLIENT_CONNECTED;

	/* create a connection */
	trans->conn = p9_conn_create(client);
	if (IS_ERR(trans->conn)) {
		ret = PTR_ERR(trans->conn);
		trans->conn = NULL;
		fput(trans->filp);
		kfree(trans);
		return ret;
	}

	return 0;
}

/*
 * Create TCP transport.
 */
static int p9_fd_create_tcp(struct p9_client *client, const char *addr, char *args)
{
	struct sockaddr_in sin_server;
	struct socket *sock = NULL;
	struct p9_fd_opts opts;
	int ret;

	/* parse options */
	ret = parse_opts(args, &opts);
	if (ret < 0)
		return ret;

	/* create socket */
	ret = sock_create(AF_INET, SOCK_STREAM, IP_PROTO_TCP, &sock);
	if (ret) {
		p9_error("p9_trans_tcp: problem creating socket\n");
		return ret;
	}

	/* connect to server */
	sin_server.sin_family = AF_INET;
	sin_server.sin_addr = in_aton(addr);
	sin_server.sin_port = htons(opts.port);
	ret = sock->ops->connect(sock, (struct sockaddr *) &sin_server, sizeof(struct sockaddr_in), 0);
	if (ret < 0) {
		p9_error("p9_trans_tcp: problem connecting socket to %s = %d\n", addr, ret);
		sock_release(sock);
		return ret;
	}

	/* open socket */
	return p9_socket_open(client, sock);
}

/*
 * Close TCP transport.
 */
static void p9_fd_close(struct p9_client *client)
{
	struct p9_trans_fd *trans;

	if (!client)
		return;

	/* get transport module */
	trans = client->trans;
	if (!trans)
		return;

	/* set client disconnected */
	client->status = P9_CLIENT_DISCONNECTED;

	/* destroy connection */
	p9_conn_destroy(trans->conn);

	/* release file */
	if (trans->filp)
		fput(trans->filp);

	/* free transport module */
	kfree(trans);
}

/*
 * Create a TCP request.
 */
static int p9_fd_request(struct p9_client *client, struct p9_request *req)
{
	struct p9_trans_fd *trans = client->trans;
	struct p9_conn *conn = trans->conn;
	int n;

	/* error on connection */
	if (conn->err < 0)
		return conn->err;

	/* add request */
	req->status = P9_REQUEST_STATUS_UNSENT;
	list_add_tail(&req->list, &conn->unsent_req_list);

	/* poll connection */
	if (test_and_clear_bit(&conn->wsched, WPENDING))
		n = POLLOUT;
	else
		n = p9_fd_poll(conn->client, NULL);

	/* schedule write work */
	if ((n & POLLOUT) && !test_and_set_bit(&conn->wsched, WWORKSCHED))
		queue_task(&conn->wq);

	return 0;
}

/*
 * Cancel a request.
 */
static int p9_fd_cancel(struct p9_client *client, struct p9_request *req)
{
	/* print a debug message */
	p9_debug("client %x req %x\n", client, req);

	/* request not sent : just delete it */
	if (req->status == P9_REQUEST_STATUS_UNSENT) {
		list_del(&req->list);
		req->status = P9_REQUEST_STATUS_FLSHD;
		return 0;
	}

	if (req->status == P9_REQUEST_STATUS_SENT)
		req->status = P9_REQUEST_STATUS_FLSHD;

	return 1;
}

/*
 * 9p TCP transport.
 */
static struct p9_trans_module p9_tcp_trans = {
	.name 		= "tcp",
	.maxsize 	= MAX_SOCK_BUF,
	.create 	= p9_fd_create_tcp,
	.close 		= p9_fd_close,
	.cancel		= p9_fd_cancel,
	.request	= p9_fd_request,
};

/*
 * Init file transport.
 */
int p9_trans_fd_init()
{
	/* register tcp module */
	v9fs_register_trans(&p9_tcp_trans);

	/* init poll task queue */
	INIT_TQUEUE(&poll_tq, p9_poll_work, NULL);

	return 0;
}
