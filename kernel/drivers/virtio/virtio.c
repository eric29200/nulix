#include <drivers/virtio/virtio.h>
#include <drivers/virtio/virtio_rng.h>

/*
 * Init virtio.
 */
int init_virtio()
{
	return init_virtio_rng();
}
