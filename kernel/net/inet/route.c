#include <net/inet/route.h>
#include <net/inet/net.h>
#include <net/inet/in.h>
#include <net/inet/ip.h>
#include <stdio.h>
#include <stderr.h>

/*
 * Add a route.
 */
int ip_route_new(struct rtentry *rt)
{
	struct net_device *net_dev;

	/* check destination address */
	if (rt->rt_dst.sa_family != AF_INET)
		return -EAFNOSUPPORT;

	/* interface must be specified */
	if (!rt->rt_dev)
		return -ENODEV;

	/* find network device */
	net_dev = net_device_find(rt->rt_dev);
	if (!net_dev)
		return -ENODEV;

	/* set gateway */
	if (rt->rt_flags & RTF_GATEWAY) {
		inet_ntoi(((struct sockaddr_in *) &rt->rt_gateway)->sin_addr, net_dev->ip_route);
		return 0;
	}

	return -EINVAL;
}
