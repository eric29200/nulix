#include <net/ip.h>
#include <string.h>

/*
 * Receive an IP packet.
 */
void ip_receive_packet(struct net_device_t *net_dev, void *packet, size_t packet_len)
{
  struct ip_packet_t *ip_packet;

  /* get IP packet */
  ip_packet = (struct ip_packet_t *) packet;
  if (packet_len < sizeof(struct ip_packet_t))
    return;

  /* IPv4 only */
  if (ip_packet->version != 4)
    return;

  /* check if message is adressed to us */
  if (memcmp(net_dev->ip_addr, ip_packet->dst_addr, 4) != 0)
    return;

  /* handle ICMP packet */
  if (ip_packet->protocol == IP_PROTO_ICMP)
    icmp_receive_packet(net_dev, packet + sizeof(struct ip_packet_t), packet_len - sizeof(struct ip_packet_t));
}
