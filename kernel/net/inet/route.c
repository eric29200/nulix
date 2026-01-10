#include <net/inet/route.h>
#include <net/inet/in.h>
#include <net/inet/net.h>
#include <mm/mm.h>
#include <net/if.h>
#include <stdio.h>
#include <stderr.h>
#include <string.h>
#include <lib/list.h>

/* route entries */
static LIST_HEAD(rt_entries);

/*
 * Check if a mask is acceptable.
 */
static int bad_mask(uint32_t mask, uint32_t addr)
{
	if (addr & (mask = ~mask))
		return 1;
	mask = ntohl(mask);
	if (mask & (mask + 1))
		return 1;
	return 0;
}

/*
 * Read route.
 */
int route_read_proc(char *page, char **start, off_t off, size_t count, int *eof)
{
	struct list_head *pos;
	struct route *rt;
	size_t len;

	/* print header */
	len = sprintf(page, "Iface\tDestination\tGateway \tFlags\tRefCnt\tUse\tMetric\tMask\t\tMTU\tWindow\tIRTT\n");

	/* print routes */
	list_for_each(pos, &rt_entries) {
		rt = list_entry(pos, struct route, rt_list);
		len += sprintf(page + len, "%s\t%08lX\t%08lX\t%04X\t%d\t\t%lu\t%d\t\t%08lX\t%d\t%lu\t\t%u\n",
					rt->rt_dev->name,
					rt->rt_dst,
					rt->rt_gateway,
					rt->rt_flags,
					0,
					rt->rt_use,
					rt->rt_metric,
					rt->rt_mask,
					rt->rt_mtu,
					rt->rt_window,
					rt->rt_irtt);
	}

	/* end of file ? */
	if (off + count >= len)
		*eof = 1;

	/* set start buffer */
	*start = page + off;

	/* check offset */
	if (off >= len)
		return 0;

	/* compute length */
	len -= off;
	if (len > count)
		len = count;

	return len;
}

/*
 * Add a route.
 */
static int rt_add(uint16_t flags, uint32_t dst, uint32_t mask, uint32_t gw, struct net_device *dev, uint16_t mss,
	uint32_t window, uint16_t irtt, uint16_t metric)
{
	struct route *rt;

	rt = (struct route *) kmalloc(sizeof(struct route));
	if (!rt)
		return -ENOMEM;

	/* set route */
	memset(rt, 0, sizeof(struct route));
	rt->rt_dst = dst;
	rt->rt_metric = metric;
	rt->rt_gateway = gw;
	rt->rt_mask = mask;
	rt->rt_dev = dev;
	rt->rt_window = window;
	rt->rt_flags = flags;
	rt->rt_irtt = irtt;
	rt->rt_mtu = mss;

	/* add route */
	list_add_tail(&rt->rt_list, &rt_entries);

	return 0;
}

/*
 * Create a new route.
 */
static int ip_rt_new(struct rtentry *rt)
{
	struct net_device *net_dev = NULL;
	uint32_t daddr, mask, gw, flags;
	uint16_t metric;

	/* check destination address */
	if (rt->rt_dst.sa_family != AF_INET)
		return -EAFNOSUPPORT;

	/* get network device */
	if (!rt->rt_dev)
		return -ENODEV;

	/* find device */
	net_dev = net_device_find(rt->rt_dev);
	if (!net_dev)
		return -ENODEV;

	/* parse route */
	flags = rt->rt_flags;
	daddr = ((struct sockaddr_in *) &rt->rt_dst)->sin_addr;
	mask = ((struct sockaddr_in *) &rt->rt_genmask)->sin_addr;
	gw = ((struct sockaddr_in *) &rt->rt_gateway)->sin_addr;
	metric = rt->rt_metric > 0 ? rt->rt_metric - 1 : 0;

	/* check mask */
	if (flags & RTF_HOST)
		mask = 0xFFFFFFFF;
	else if (mask && rt->rt_genmask.sa_family != AF_INET)
		return -EAFNOSUPPORT;

	/* check mask */
	if (bad_mask(mask, daddr))
		return -EINVAL;

	/* add route */
	return rt_add(flags, daddr, mask, gw, net_dev, rt->rt_mss, rt->rt_window, rt->rt_irtt, metric);
}

/*
 * Find a route.
 */
struct route *ip_rt_route(uint32_t daddr)
{
	struct route *rt, *res_gw = NULL;
	struct list_head *pos;

	/* find local route or gateway */
	list_for_each(pos, &rt_entries) {
		rt = list_entry(pos, struct route, rt_list);

		if ((rt->rt_dst & rt->rt_mask) == (daddr & rt->rt_mask)) {
			if (rt->rt_flags & RTF_GATEWAY)
				res_gw = rt;
			else
				return rt;
		}
	}

	return res_gw;
}

/*
 * Route ioctl.
 */
int ip_rt_ioctl(int cmd, void *arg)
{
	switch (cmd) {
		case SIOCADDRT:
			return ip_rt_new((struct rtentry *) arg);
		default:
			return -EINVAL;
	}
}
