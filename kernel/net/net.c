#include <net/net.h>

/* network devices */
static struct net_device_t net_devices[NR_NET_DEVICES];
static int nb_net_devices = 0;

/*
 * Register a newtork device.
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

  return net_dev;
}
