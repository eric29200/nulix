#include <net/9p/9p.h>
#include <net/socket.h>
#include <net/inet/ip.h>
#include <net/inet/in.h>
#include <net/inet/net.h>
#include <fs/fs.h>
#include <stdio.h>
#include <stderr.h>
#include <mm/mm.h>
#include <string.h>

#define MAX_SOCK_BUF		(64 * 1024)

/*
 * File transport.
 */
struct p9_trans_fd {
	struct file *	filp;
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
 * Attach socket to the client.
 */
static int p9_socket_open(struct p9_client *client, struct socket *sock)
{
	struct p9_trans_fd *trans;
	struct file *filp;

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
	trans->filp = filp;
	sock->file = filp;
	client->trans = trans;
	client->status = P9_CLIENT_CONNECTED;

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

	trans = client->trans;
	if (!trans)
		return;

	/* set client disconnected */
	client->status = P9_CLIENT_DISCONNECTED;

	/* free transport */
	fput(trans->filp);
	kfree(trans);
}

/*
 * Create a TCP request.
 */
static int p9_fd_request(struct p9_client *client, struct p9_request *req)
{
	UNUSED(client);
	UNUSED(req);
	printf("TODO: p9_fd_request\n");
	return -EINVAL;
}

/*
 * 9p TCP transport.
 */
static struct p9_trans_module p9_tcp_trans = {
	.name 		= "tcp",
	.maxsize 	= MAX_SOCK_BUF,
	.create 	= p9_fd_create_tcp,
	.close 		= p9_fd_close,
	.request	= p9_fd_request,
};

/*
 * Init file transport.
 */
int p9_trans_fd_init()
{
	v9fs_register_trans(&p9_tcp_trans);
	return 0;
}