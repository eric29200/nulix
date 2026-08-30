#include <drivers/virtio/virtio.h>
#include <drivers/virtio/virtio_rng.h>
#include <mm/paging.h>
#include <string.h>
#include <stderr.h>

/*
 * Setup virtual queue.
 */
int setup_vq(struct virtio_device *vdev)
{
        size_t size, used_off;
        int order;

        /* select queue 0 */
        outw(vdev->io_base + VIRTIO_PCI_QUEUE_SEL, 0);

        /* check if queue is either not available or already active */
        vdev->vq.num = inw(vdev->io_base + VIRTIO_PCI_QUEUE_SIZE);
        if (!vdev->vq.num || inl(vdev->io_base + VIRTIO_PCI_QUEUE_PFN))
                return -ENOENT;

        /* fix queue size */
        if (vdev->vq.num > 256)
                vdev->vq.num = 256;

        /* compute size of virtual queue */
        size = PAGE_ALIGN_UP(vring_size(vdev->vq.num));
        order = get_order(size);

        /* allocate queue */
        vdev->vq.queue = get_free_pages(order);
        if (!vdev->vq.queue)
                return -ENOMEM;

        /* clear queue */
        memset((void *) vdev->vq.queue, 0, size);

        /* setup queue */
        vdev->vq.desc  = (struct vring_desc *) vdev->vq.queue;
        vdev->vq.avail = (struct vring_avail *) (vdev->vq.queue + vdev->vq.num * sizeof(struct vring_desc));
        used_off = (uint32_t) vdev->vq.num * sizeof(struct vring_desc) + sizeof(uint16_t) * (2 + vdev->vq.num);
        used_off = (used_off + (VIRTIO_QUEUE_ALIGN - 1)) & ~(uint32_t) (VIRTIO_QUEUE_ALIGN - 1);
        vdev->vq.used = (struct vring_used *) (vdev->vq.queue + used_off);
        vdev->vq.last_used_idx = 0;

        /* activate queue */
        outl(vdev->io_base + VIRTIO_PCI_QUEUE_PFN, (uint32_t) (__pa(vdev->vq.queue) >> VIRTIO_QUEUE_SHIFT));

        return 0;
}

/*
 * Init virtio.
 */
int init_virtio()
{
	return init_virtio_rng();
}
