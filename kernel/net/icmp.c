#include <net/icmp.h>

/*
 * Handle an ICMP packet.
 */
void icmp_receive_packet(struct net_device_t *net_dev, void *packet, size_t packet_len)
{
  struct icmp_packet_t *icmp_packet;

  /* get ICMP packet */
  icmp_packet = (struct icmp_packet_t *) packet;
  if (packet_len < sizeof(struct icmp_packet_t))
    return;

  /* handle echo request */
  if (icmp_packet->type == ICMP_TYPE_REQUEST && icmp_packet->code == ICMP_CODE_REQUEST)
    printf("ok\n");
}
