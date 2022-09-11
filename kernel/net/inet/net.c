#include <net/inet/net.h>
#include <net/inet/sock.h>
#include <net/inet/ethernet.h>
#include <net/inet/arp.h>
#include <net/inet/ip.h>
#include <net/inet/icmp.h>
#include <net/inet/udp.h>
#include <net/inet/tcp.h>
#include <proc/sched.h>

/* network devices */
static struct net_device_t net_devices[NR_NET_DEVICES];
static int nb_net_devices = 0;

/* sockets (defined in socket.c) */
extern struct socket_t sockets[NR_SOCKETS];

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
 * Deliver a packet to sockets.
 */
static void skb_deliver_to_sockets(struct sk_buff_t *skb)
{
	struct sock_t *sk;
	int i, ret;

	/* find matching sockets */
	for (i = 0; i < NR_SOCKETS; i++) {
		/* free or non inet socket */
		if (sockets[i].state == SS_FREE || sockets[i].family != AF_INET)
			continue;

		/* get inet socket */
		sk = (struct sock_t *) sockets[i].data;
		if (!sk)
			continue;

		/* handle packet */
		if (sk->prot && sk->prot->handle) {
			ret = sk->prot->handle(sk, skb);

			/* wake up waiting processes */
			if (ret == 0)
				task_wakeup_all(&sk->sock->waiting_chan);
		}
	}
}

/*
 * Handle a socket buffer.
 */
void skb_handle(struct sk_buff_t *skb)
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
			skb_deliver_to_sockets(skb);

			break;
		default:
			break;
	}
}

/*
 * Network handler thread.
 */
static void net_handler_thread(void *arg)
{
	struct net_device_t *net_dev = (struct net_device_t *) arg;
	struct list_head_t *pos1, *n1, *pos2, *n2;
	struct sk_buff_t *skb;
	uint32_t flags;
	int ret;

	for (;;) {
		/* disable interrupts */
		irq_save(flags);

		/* handle incoming packets */
		list_for_each_safe(pos1, n1, &net_dev->skb_input_list) {
			/* get packet */
			skb = list_entry(pos1, struct sk_buff_t, list);
			list_del(&skb->list);

			/* handle packet */
			skb_handle(skb);

			/* free packet */
			skb_free(skb);
		}

		/* handle outcoming packets */
		list_for_each_safe(pos2, n2, &net_dev->skb_output_list) {
			/* get packet */
			skb = list_entry(pos2, struct sk_buff_t, list);

			/* rebuild ethernet header */
			ret = ethernet_rebuild_header(net_dev, skb);

			/* send packet and remove it from list */
			if (ret == 0) {
				list_del(&skb->list);
				net_dev->send_packet(skb);
				skb_free(skb);
			}
		}

		/* enable interrupts */
		irq_restore(flags);

		/* wait for incoming packets */
		current_task->timeout = jiffies + ms_to_jiffies(NET_HANDLE_FREQ_MS);
		task_sleep(current_task->waiting_chan);
		current_task->timeout = 0;
	}
}

/*
 * Register a network device.
 */
struct net_device_t *register_net_device(uint32_t io_base)
{
	struct net_device_t *net_dev;

	/* network devices table full */
	if (nb_net_devices >= NR_NET_DEVICES)
		return NULL;

	/* set net device */
	net_dev = &net_devices[nb_net_devices++];
	net_dev->io_base = io_base;
	INIT_LIST_HEAD(&net_dev->skb_input_list);
	INIT_LIST_HEAD(&net_dev->skb_output_list);

	/* create kernel thread to handle receive packets */
	net_dev->thread = create_kernel_thread(net_handler_thread, net_dev);
	if (!net_dev->thread)
		return NULL;

	return net_dev;
}

/*
 * Handle network packet (put it in device queue).
 */
void net_handle(struct net_device_t *net_dev, struct sk_buff_t *skb)
{
	if (!skb)
		return;

	/* put socket buffer in net device list */
	list_add_tail(&skb->list, &net_dev->skb_input_list);

	/* wake up handler */
	task_wakeup_all(net_dev->thread->waiting_chan);
}

/*
 * Transmit a network packet (put it in device queue).
 */
void net_transmit(struct net_device_t *net_dev, struct sk_buff_t *skb)
{
	if (!skb)
		return;

	/* put socket buffer in net device list */
	list_add_tail(&skb->list, &net_dev->skb_output_list);

	/* wake up handler */
	task_wakeup_all(net_dev->thread->waiting_chan);
}
