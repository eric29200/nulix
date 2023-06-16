#ifndef _UN_H_
#define _UN_H_

#define UN_PATH_OFFSET	((unsigned long)((struct sockaddr_un *) 0)->sun_path)

/*
 * UNIX socket address.
 */
struct sockaddr_un {
	uint16_t	sun_family;
	char		sun_path[108];
};

#endif
