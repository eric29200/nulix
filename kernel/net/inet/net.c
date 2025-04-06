#include <net/sock.h>
#include <net/inet/net.h>
#include <net/inet/ethernet.h>
#include <net/inet/arp.h>
#include <net/inet/ip.h>
#include <net/inet/icmp.h>
#include <net/inet/udp.h>
#include <net/inet/tcp.h>
#include <net/if.h>
#include <proc/sched.h>
#include <stdio.h>
#include <stderr.h>

/* network devices */
struct net_device net_devices[NR_NET_DEVICES];
int nr_net_devices = 0;

/*
 * Compute checksum.
 */
uint16_t net_checksum(void *data, size_t size)
{
	uint16_t *chunk, ret;
	uint32_t chksum;

	for (chksum = 0, chunk = (uint16_t *) data; size > 1; size -= 2)
		chksum += *chunk++;

	if (size == 1)
		chksum += *((uint8_t *) chunk);

	chksum = (chksum & 0xFFFF) + (chksum >> 16);
	chksum += (chksum >> 16);
	ret = ~chksum;

	return ret;
}

/*
 * Handle a socket buffer.
 */
void skb_handle(struct sk_buff *skb)
{
	/* decode ethernet header */
	ethernet_receive(skb);

	/* handle packet */
	switch(htons(skb->eth_header->type)) {
		case ETHERNET_TYPE_ARP:
			/* decode ARP header */
			arp_receive(skb);

			/* reply to ARP request or add arp table entry */
			if (ntohs(skb->nh.arp_header->opcode) == ARP_REQUEST)
				arp_reply_request(skb);
			else if (ntohs(skb->nh.arp_header->opcode) == ARP_REPLY)
				arp_add_table(skb->nh.arp_header);

			break;
		case ETHERNET_TYPE_IP:
			/* decode IP header */
			ip_receive(skb);

			/* handle IPv4 only */
			if (skb->nh.ip_header->version != 4)
				break;

			/* check if packet is adressed to us */
			if (memcmp(skb->dev->ip_addr, skb->nh.ip_header->dst_addr, 4) != 0)
				break;

			/* go to next layer */
			switch (skb->nh.ip_header->protocol) {
				case IP_PROTO_UDP:
					udp_receive(skb);
					break;
				case IP_PROTO_TCP:
					tcp_receive(skb);
					break;
				case IP_PROTO_ICMP:
					icmp_receive(skb);

					/* handle ICMP requests */
					if (skb->h.icmp_header->type == ICMP_TYPE_ECHO) {
						icmp_reply_echo(skb);
						return;
					}

					break;
				default:
					break;
			}

			/* deliver message to sockets */
			net_deliver_skb(skb);

			break;
		default:
			break;
	}
}

/*
 * Network handler timer.
 */
static void net_handler_timer(void *arg)
{
	struct net_device *net_dev = (struct net_device *) arg;
	struct list_head *pos1, *n1, *pos2, *n2;
	struct sk_buff *skb;
	uint32_t flags;
	int ret;

	/* disable interrupts */
	irq_save(flags);

	/* handle incoming packets */
	list_for_each_safe(pos1, n1, &net_dev->skb_input_list) {
		/* get packet */
		skb = list_entry(pos1, struct sk_buff, list);
		list_del(&skb->list);

		/* handle packet */
		skb_handle(skb);

		/* free packet */
		skb_free(skb);
	}

	/* handle outcoming packets */
	list_for_each_safe(pos2, n2, &net_dev->skb_output_list) {
		/* get packet */
		skb = list_entry(pos2, struct sk_buff, list);

		/* rebuild ethernet header */
		ret = ethernet_rebuild_header(net_dev, skb);

		/* send packet and remove it from list */
		if (ret == 0) {
			list_del(&skb->list);
			net_dev->send_packet(skb);
			skb_free(skb);
		}
	}

	/* wait for incoming packets = reschedule timer */
	timer_event_init(&net_dev->timer, net_handler_timer, net_dev, jiffies + ms_to_jiffies(NET_HANDLE_FREQ_MS));
	timer_event_add(&net_dev->timer);

	/* enable interrupts */
	irq_restore(flags);
}

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
	net_dev->type = type;
	net_dev->index = nr_net_devices;
	net_dev->io_base = io_base;
	net_dev->wait = NULL;
	INIT_LIST_HEAD(&net_dev->skb_input_list);
	INIT_LIST_HEAD(&net_dev->skb_output_list);

	/* set name */
	len = sprintf(tmp, "eth%d", nr_net_devices);

	/* allocate name */
	net_dev->name = (char *) kmalloc(len + 1);
	if (!net_dev->name)
		return NULL;

	/* set name */
	memcpy(net_dev->name, tmp, len + 1);

	/* create kernel thread to handle receive packets */
	/* create kernel timer to handle receive packets */
	timer_event_init(&net_dev->timer, net_handler_timer, net_dev, jiffies + ms_to_jiffies(NET_HANDLE_FREQ_MS));
	timer_event_add(&net_dev->timer);

	/* update number of net devices */
	nr_net_devices++;

	return net_dev;
}

/*
 * Find a network device.
 */
struct net_device *net_device_find(const char *name)
{
	int i;

	if (!name)
		return NULL;

	for (i = 0; i < nr_net_devices; i++)
		if (strcmp(net_devices[i].name, name) == 0)
			return &net_devices[i];

	return NULL;
}

/*
 * Network device ioctl.
 */
int net_device_ioctl(int cmd, struct ifreq *ifr)
{
	struct net_device *net_dev;

	/* get network device */
	net_dev = net_device_find(ifr->ifr_ifrn.ifrn_name);
	if (!net_dev)
		return -EINVAL;

	switch (cmd) {
		case SIOCGIFINDEX:
			ifr->ifr_ifru.ifru_ivalue = net_dev->index;
			break;
		case SIOCGIFFLAGS:
			ifr->ifr_ifru.ifru_flags = net_dev->flags;
			break;
		case SIOCSIFFLAGS:
			net_dev->flags = ifr->ifr_ifru.ifru_flags;
			break;
		case SIOCGIFADDR:
		 	((struct sockaddr_in *) &ifr->ifr_ifru.ifru_addr)->sin_addr = inet_iton(net_dev->ip_addr);
		 	((struct sockaddr_in *) &ifr->ifr_ifru.ifru_addr)->sin_family = AF_INET;
			break;
		case SIOCSIFADDR:
		 	inet_ntoi(((struct sockaddr_in *) &ifr->ifr_ifru.ifru_addr)->sin_addr, net_dev->ip_addr);
			break;
		case SIOCGIFNETMASK:
		 	((struct sockaddr_in *) &ifr->ifr_ifru.ifru_addr)->sin_addr = inet_iton(net_dev->ip_netmask);
		 	((struct sockaddr_in *) &ifr->ifr_ifru.ifru_addr)->sin_family = AF_INET;
			break;
		case SIOCSIFNETMASK:
		 	inet_ntoi(((struct sockaddr_in *) &ifr->ifr_ifru.ifru_addr)->sin_addr, net_dev->ip_netmask);
			break;
		case SIOCGIFHWADDR:
		 	memcpy(ifr->ifr_ifru.ifru_hwaddr.sa_data, net_dev->mac_addr, 6);
			ifr->ifr_ifru.ifru_hwaddr.sa_family = net_dev->type;
			break;
		default:
			printf("INET socket : unknown ioctl cmd %x\n", cmd);
			return -EINVAL;
	}

	return 0;
}

/*
 * Get network devices configuration.
 */
int net_device_ifconf(struct ifconf *ifc)
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
 * Handle network packet (put it in device queue).
 */
void net_handle(struct net_device *net_dev, struct sk_buff *skb)
{
	if (!skb)
		return;

	/* put socket buffer in net device list */
	list_add_tail(&skb->list, &net_dev->skb_input_list);

	/* wake up handler */
	wake_up(&net_dev->wait);
}

/*
 * Transmit a network packet (put it in device queue).
 */
void net_transmit(struct net_device *net_dev, struct sk_buff *skb)
{
	if (!skb)
		return;

	/* put socket buffer in net device list */
	list_add_tail(&skb->list, &net_dev->skb_output_list);

	/* wake up handler */
	wake_up(&net_dev->wait);
}
