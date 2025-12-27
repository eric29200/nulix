#ifndef _AF_UNIX_H_
#define _AF_UNIX_H_

#include <net/sock.h>

typedef struct sock unix_socket_t;

/*
 * Socket buffer parameters.
 */
struct unix_skb_parms {
	uint32_t		attr;
};

#define UNIXCB(skb) 	(*(struct unix_skb_parms *) &((skb)->cb))

#endif
