#ifndef _UN_H_
#define _UN_H_

/*
 * UNIX socket address.
 */
struct sockaddr_un {
	uint16_t		sun_family;
	char			sun_path[108];
};

/*
 * UNIX address.
 */
struct unix_address {
	int			refcnt;
	size_t			len;
	struct sockaddr_un 	name[0];
};

#endif
