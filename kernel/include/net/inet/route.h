#ifndef _ROUTE_H_
#define _ROUTE_H_

#include <net/socket.h>

#define	RTF_GATEWAY	0x0002

/*
 * Route entry.
 */
struct rtentry {
	unsigned long	rt_pad1;
	struct sockaddr	rt_dst;
	struct sockaddr	rt_gateway;
	struct sockaddr	rt_genmask;
	unsigned short	rt_flags;
	short		rt_pad2;
	unsigned long	rt_pad3;
	void *		rt_pad4;
	short		rt_metric;
	char *		rt_dev;
	unsigned long	rt_mtu;
	unsigned long	rt_window;
	unsigned short	rt_irtt;
};

#endif
