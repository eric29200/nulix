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
	size_t len;
	int i;

	/* print header */
	len = sprintf(page, "Inter-|   Receive                            "
		"                    |  Transmit\n"
		" face |bytes    packets errs drop fifo frame "
		"compressed multicast|bytes    packets errs "
		"drop fifo colls carrier compressed\n");

	/* print interfaces */
	for (i = 0; i < nr_net_devices; i++)
		len += sprintf(page + len, "%s: 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n", net_devices[i].name);

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
	struct net_device *net_dev;

	/* get network device */
	net_dev = net_device_find(ifr->ifr_ifrn.ifrn_name);
	if (!net_dev)
		return -EINVAL;

	switch (cmd) {
		case SIOCGIFFLAGS:
			ifr->ifr_ifru.ifru_flags = net_dev->flags;
			break;
		case SIOCGIFHWADDR:
		 	memcpy(ifr->ifr_ifru.ifru_hwaddr.sa_data, net_dev->mac_addr, 6);
			ifr->ifr_ifru.ifru_hwaddr.sa_family = net_dev->type;
			break;
		case SIOCGIFINDEX:
			ifr->ifr_ifru.ifru_ivalue = net_dev->index;
			break;
		case SIOCGIFMETRIC:
			ifr->ifr_ifru.ifru_ivalue = 0;
			break;
		case SIOCGIFMTU:
			ifr->ifr_ifru.ifru_mtu = net_dev->mtu;
			break;
		case SIOCGIFTXQLEN:
			ifr->ifr_ifru.ifru_ivalue = net_dev->tx_queue_len;
			break;
		case SIOCSIFFLAGS:
			net_dev->flags = ifr->ifr_ifru.ifru_flags;
			break;
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
		(*(struct sockaddr_in *) &ifr.ifr_ifru.ifru_addr).sin_addr = inet_iton(net_devices[i].ip_addr);

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