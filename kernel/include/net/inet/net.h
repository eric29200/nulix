#ifndef _NET_H_
#define _NET_H_

#include <net/sk_buff.h>
#include <net/if.h>
#include <proc/timer.h>

#define NR_NET_DEVICES			4
#define MAX_ADDR_LEN			7

#define IS_MYADDR			1		/* address is (one of) our own */
#define IS_LOOPBACK			2		/* address is for LOOPBACK */
#define IS_BROADCAST			3		/* address is a valid broadcast	*/
#define IS_INVBCAST			4		/* Wrong netmask bcast not for us (unused) */
#define IS_MULTICAST			5		/* Multicast IP address */

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
	uint16_t		family;
	uint8_t			index;
	uint32_t		io_base;
	uint8_t			irq;
	uint8_t			hw_addr[MAX_ADDR_LEN];
	uint32_t		ip_addr;
	uint32_t		ip_netmask;
	size_t			hard_header_len;
	size_t			addr_len;
	uint16_t		type;
	uint16_t		flags;
	uint32_t		mtu;
	size_t			tx_queue_len;
	struct net_device_stats	stats;
	void *			private;
	int			(*start_xmit)(struct sk_buff *, struct net_device *);
	void 			(*hard_header)(struct sk_buff *, uint16_t, uint8_t *, uint8_t *);
	int			(*rebuild_header)(struct net_device *, uint32_t, struct sk_buff *);
};

/* network devices */
extern struct net_device net_devices[NR_NET_DEVICES];
extern int nr_net_devices;

/* network prototypes */
int init_net_dev();
struct net_device *register_net_device(uint32_t io_base, uint16_t type, uint16_t family, const char *name);
struct net_device *net_device_find(const char *name);
int dev_ioctl(unsigned int cmd, void *arg);
void skb_handle(struct sk_buff *skb);
uint16_t net_checksum(void *data, size_t size);
void net_transmit(struct net_device *dev, struct sk_buff *skb);
void net_deliver_skb(struct sk_buff *skb);

void destroy_sock(struct sock *sk);

/*
 * Convert an ASCII string to binary IP.
 */
static inline uint32_t in_aton(const char *str)
{
	uint32_t val, l;
	int i;

	for (i = 0, l = 0; i < 4; i++) {
		l <<= 8;

		if (*str) {
			val = 0;
			while (*str && *str != '.') {
				val *= 10;
				val += *str - '0';
				str++;
			}
			l |= val;
			if (*str)
				str++;
		}
	}

	return htonl(l);
}

#endif
