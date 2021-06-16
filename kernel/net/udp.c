#include <net/udp.h>

/*
 * Receive/decode an UDP packet.
 */
void udp_receive(struct sk_buff_t *skb)
{
  skb->h.udp_header = (struct udp_header_t *) skb->data;
  skb_pull(skb, sizeof(struct udp_header_t));
}
