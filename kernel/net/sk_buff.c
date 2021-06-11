#include <net/sk_buff.h>
#include <mm/mm.h>
#include <string.h>

/*
 * Allocate a socket buffer.
 */
struct sk_buff_t *skb_alloc(size_t size)
{
  struct sk_buff_t *skb;
  uint8_t *data;

  /* allocate socket buffer */
  skb = (struct sk_buff_t *) kmalloc(sizeof(struct sk_buff_t));
  if (!skb)
    return NULL;
  memset(skb, 0, sizeof(struct sk_buff_t));

  /* allocate data */
  skb->size = size;
  data = (uint8_t *) kmalloc(size);
  if (!data) {
    kfree(skb);
    return NULL;
  }

  /* set socket buffer */
  memset(data, 0, size);
  skb->head = data;
  skb->data = data;
  skb->tail = data;
  skb->end = data + size;
  skb->len = 0;

  return skb;
}

/*
 * Free a socket buffer.
 */
void skb_free(struct sk_buff_t *skb)
{
  kfree(skb->head);
  kfree(skb);
}
