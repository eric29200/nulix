#ifndef _NET_H_
#define _NET_H_

#include <net/sk_buff.h>
#include <net/if.h>
#include <proc/timer.h>

#define NR_NET_DEVICES			4

#define htons(s)			((((s) & 0xFF) << 8) | (((s) & 0xFF00) >> 8))
#define htonl(l)			((((l) & 0xFF) << 24) | (((l) & 0xFF00) << 8) | (((l) & 0xFF0000) >> 8) | (((l) & 0xFF000000) >> 24))
#define ntohs(s)			(htons((s)))
#define ntohl(s)			(htonl((s)))

/*
 * Networkd device statistics.
 */
struct net_device_stats {
	uint32_t		rx_packets;		/* total packets received	*/
	uint32_t		tx_packets;		/* total packets transmitted	*/
	uint32_t		rx_bytes;		/* total bytes received 	*/
	uint32_t		tx_bytes;		/* total bytes transmitted	*/
	uint32_t		rx_errors;		/* bad packets received		*/
	uint32_t		tx_errors;		/* packet transmit problems	*/
	uint32_t		rx_dropped;		/* no space in linux buffers	*/
	uint32_t		tx_dropped;		/* no space available in linux	*/
	uint32_t		multicast;		/* multicast packets received	*/
	uint32_t		collisions;
	/* detailed rx_errors: */
	uint32_t		rx_length_errors;
	uint32_t		rx_over_errors;		/* receiver ring buff overflow	*/
	uint32_t		rx_crc_errors;		/* recved pkt with crc error	*/
	uint32_t		rx_frame_errors;	/* recv'd frame alignment error */
	uint32_t		rx_fifo_errors;		/* recv'r fifo overrun		*/
	uint32_t		rx_missed_errors;	/* receiver missed packet	*/
	/* detailed tx_errors */
	uint32_t		tx_aborted_errors;
	uint32_t		tx_carrier_errors;
	uint32_t		tx_fifo_errors;
	uint32_t		tx_heartbeat_errors;
	uint32_t		tx_window_errors;
	/* for cslip etc */
	uint32_t		rx_compressed;
	uint32_t		tx_compressed;
};

/*
 * Network device.
 */
struct net_device {
	char *			name;
	uint8_t			index;
	uint32_t		io_base;
	uint8_t			irq;
	uint8_t			mac_addr[6];
	uint32_t		ip_addr;
	uint32_t		ip_netmask;
	uint32_t		ip_route;
	uint16_t		type;
	uint16_t		flags;
	uint32_t		mtu;
	size_t			tx_queue_len;
	struct net_device_stats	stats;
	void *			private;
	int			(*start_xmit)(struct sk_buff *, struct net_device *);
};

/* network devices */
extern struct net_device net_devices[NR_NET_DEVICES];
extern int nr_net_devices;

/* network prototypes */
int init_net_dev();
struct net_device *register_net_device(uint32_t io_base, uint16_t type);
struct net_device *net_device_find(const char *name);
int dev_ioctl(unsigned int cmd, void *arg);
void skb_handle(struct sk_buff *skb);
uint16_t net_checksum(void *data, size_t size);
void net_handle(struct sk_buff *skb);
void net_transmit(struct net_device *net_dev, struct sk_buff *skb);
void net_deliver_skb(struct sk_buff *skb);

#endif
