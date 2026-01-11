#include <net/inet/net.h>
#include <net/inet/ip.h>
#include <net/sock.h>
#include <fs/proc_fs.h>
#include <stdio.h>
#include <string.h>
#include <stderr.h>
#include <fcntl.h>

/*
 * Read net dev.
 */
static int dev_read_proc(char *page, char **start, off_t off, size_t count, int *eof)
{
	struct net_device_stats *stats;
	struct net_device *dev;
	size_t len;
	int i;

	/* print header */
	len = sprintf(page, "Inter-|   Receive                            "
		"                    |  Transmit\n"
		" face |bytes    packets errs drop fifo frame "
		"compressed multicast|bytes    packets errs "
		"drop fifo colls carrier compressed\n");

	/* print interfaces */
	for (i = 0; i < nr_net_devices; i++) {
		dev = &net_devices[i];
		stats = &dev->stats;
		len += sprintf(page + len, "%6s:%8lu %7lu %4lu %4lu %4lu %5lu %10lu %9lu %8lu %7lu %4lu %4lu %4lu %5lu %7lu %10lu\n",
			dev->name,
			stats->rx_bytes,
			stats->rx_packets,
			stats->rx_errors,
			stats->rx_dropped + stats->rx_missed_errors,
			stats->rx_fifo_errors,
			stats->rx_length_errors + stats->rx_over_errors + stats->rx_crc_errors + stats->rx_frame_errors,
			stats->rx_compressed,
			stats->multicast,
			stats->tx_bytes,
			stats->tx_packets,
			stats->tx_errors,
			stats->tx_dropped,
			stats->tx_fifo_errors,
			stats->collisions,
			stats->tx_carrier_errors + stats->tx_aborted_errors + stats->tx_window_errors + stats->tx_heartbeat_errors,
			stats->tx_compressed);
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
 * Network device ioctl.
 */
static int dev_ifsioc(struct ifreq *ifr, unsigned int cmd)
{
	struct net_device *dev;

	/* get network device */
	dev = net_device_find(ifr->ifr_ifrn.ifrn_name);
	if (!dev)
		return -EINVAL;

	switch (cmd) {
		case SIOCGIFFLAGS:
			ifr->ifr_ifru.ifru_flags = dev->flags;
			break;
		case SIOCGIFHWADDR:
		 	memcpy(ifr->ifr_ifru.ifru_hwaddr.sa_data, dev->hw_addr, dev->addr_len);
			ifr->ifr_ifru.ifru_hwaddr.sa_family = dev->type;
			break;
		case SIOCGIFINDEX:
			ifr->ifr_ifru.ifru_ivalue = dev->index;
			break;
		case SIOCGIFMETRIC:
			ifr->ifr_ifru.ifru_ivalue = 0;
			break;
		case SIOCGIFMTU:
			ifr->ifr_ifru.ifru_mtu = dev->mtu;
			break;
		case SIOCGIFTXQLEN:
			ifr->ifr_ifru.ifru_ivalue = dev->tx_queue_len;
			break;
		case SIOCSIFFLAGS:
			dev->flags = ifr->ifr_ifru.ifru_flags;
			break;
		default:
			return -EINVAL;
	}

	return 0;
}

/*
 * Get network devices configuration.
 */
int dev_ifconf(struct ifconf *ifc)
{
	char *pos = ifc->ifc_ifcu.ifcu_buf;
	size_t len = ifc->ifc_len;
	struct ifreq ifr;
	int i;

	/* for each network device */
	for (i = 0; i < nr_net_devices; i++) {
		if (len < sizeof(struct ifreq))
			break;

		/* set name/address */
		memset(&ifr, 0, sizeof(struct ifreq));
		strcpy(ifr.ifr_ifrn.ifrn_name, net_devices[i].name);
		(*(struct sockaddr_in *) &ifr.ifr_ifru.ifru_addr).sin_family = AF_INET;
		(*(struct sockaddr_in *) &ifr.ifr_ifru.ifru_addr).sin_addr = net_devices[i].ip_addr;

		/* copy to ifconf */
		memcpy(pos, &ifr, sizeof(struct ifreq));
		pos += sizeof(struct ifreq);
		len -= sizeof(struct ifreq);
	}

	/* set length */
	ifc->ifc_len = pos - ifc->ifc_ifcu.ifcu_buf;
	ifc->ifc_ifcu.ifcu_req = (struct ifreq *) ifc->ifc_ifcu.ifcu_buf;

	return 0;
}

/*
 * Network device ioctl.
 */
int dev_ioctl(unsigned int cmd, void *arg)
{
	switch (cmd) {
		case SIOCGIFCONF:
			return dev_ifconf((struct ifconf *) arg);
		case SIOCGIFFLAGS:
		case SIOCGIFHWADDR:
		case SIOCGIFINDEX:
		case SIOCGIFMETRIC:
		case SIOCGIFMTU:
		case SIOCGIFTXQLEN:
		case SIOCSIFFLAGS:
			return dev_ifsioc((struct ifreq *) arg, cmd);
		default:
			printf("dev_ioctl : unknown ioctl cmd 0x%x\n", cmd);
			return -EINVAL;
	}
}

/*
 * Init network devices.
 */
int init_net_dev()
{
	struct proc_dir_entry *de;

	/* register net/dev procfs entry */
	de = create_proc_read_entry("dev", 0, proc_net, dev_read_proc);
	if (!de)
		return -EINVAL;

	return 0;
}
