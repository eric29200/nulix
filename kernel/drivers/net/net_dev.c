#include <net/sock.h>
#include <drivers/net/net_dev.h>
#include <net/if.h>
#include <proc/sched.h>
#include <stdio.h>
#include <stderr.h>

/* network devices */
struct net_device net_devices[NR_NET_DEVICES];
size_t nr_net_devices = 0;

/*
 * Register a network device.
 */
struct net_device *register_net_device(uint32_t io_base, uint16_t type)
{
	struct net_device *net_dev;
	char tmp[32];
	size_t len;

	/* network devices table full */
	if (nr_net_devices >= NR_NET_DEVICES)
		return NULL;

	/* set net device */
	net_dev = &net_devices[nr_net_devices];
	net_dev->index = nr_net_devices + 1;
	net_dev->io_base = io_base;
	net_dev->type = type;
	net_dev->flags = 0;
	net_dev->mtu = 1500;
	net_dev->tx_queue_len = 100;
	memset(&net_dev->stats, 0, sizeof(struct net_device_stats));

	/* set name */
	len = sprintf(tmp, "eth%d", nr_net_devices);

	/* allocate name */
	net_dev->name = (char *) kmalloc(len + 1);
	if (!net_dev->name)
		return NULL;

	/* set name */
	memcpy(net_dev->name, tmp, len + 1);

	/* update number of net devices */
	nr_net_devices++;

	return net_dev;
}

/*
 * Find a network device.
 */
struct net_device *net_device_find(const char *name)
{
	size_t i;

	if (!name)
		return NULL;

	for (i = 0; i < nr_net_devices; i++)
		if (strcmp(net_devices[i].name, name) == 0)
			return &net_devices[i];

	return NULL;
}