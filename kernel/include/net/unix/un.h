#ifndef _UN_H_
#define _UN_H_

#include <net/socket.h>
#include <lib/list.h>

#define UN_PATH_OFFSET	((unsigned long)((struct sockaddr_un *) 0)->sun_path)

/*
 * UNIX socket address.
 */
struct sockaddr_un {
	uint16_t	sun_family;
	char		sun_path[108];
};

/*
 * UNIX socket.
 */
struct unix_sock_t {
	uint16_t		protocol;
	struct sockaddr_un	sunaddr;
	size_t			sunaddr_len;
	struct inode_t *	inode;
	struct socket_t *	sock;
	struct unix_sock_t *	other;
	off_t			msg_position;
	struct list_head_t	skb_list;
};

#endif
