#include <net/9p/9p.h>
#include <stdio.h>
#include <stderr.h>

#define MAX_SOCK_BUF		(64 * 1024)

/*
 * Create TCP transport.
 */
static int p9_fd_create_tcp(struct p9_client *client, const char *addr, char *args)
{
	UNUSED(client);
	UNUSED(addr);
	UNUSED(args);
	printf("TODO: p9_fd_create_tcp\n");
	return -EINVAL;
}

/*
 * Close TCP transport.
 */
static void p9_fd_close(struct p9_client *client)
{
	UNUSED(client);
	printf("TODO: p9_fd_close\n");
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