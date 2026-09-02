#include <drivers/virtio/virtio.h>
#include <drivers/virtio/virtio_rng.h>
#include <mm/paging.h>
#include <fs/fs.h>
#include <string.h>
#include <stderr.h>

/*
 * Setup virtual queue.
 */
struct virtqueue *setup_vq(struct virtio_device *vdev)
{
        size_t size, used_off;
        struct virtqueue *vq;
        int ret;

        /* select queue 0 */
        outw(vdev->io_base + VIRTIO_PCI_QUEUE_SEL, 0);

        /* allocate a new virt queue */
        vq = (struct virtqueue *) kmalloc(sizeof(struct virtqueue));
        if (!vq)
                return ERR_PTR(-ENOMEM);

        /* check if queue is either not available or already active */
        vq->num = inw(vdev->io_base + VIRTIO_PCI_QUEUE_SIZE);
        if (!vq->num || inl(vdev->io_base + VIRTIO_PCI_QUEUE_PFN)) {
                ret = -ENOENT;
                goto err;
        }

        /* fix queue size */
        if (vq->num > 256)
                vq->num = 256;

        /* compute size of virtual queue */
        size = PAGE_ALIGN_UP(vring_size(vq->num));
        vq->queue_order = get_order(size);

        /* allocate queue */
        vq->queue = get_free_pages(vq->queue_order);
        if (!vq->queue) {
                ret = -ENOMEM;
                goto err;
        }

        /* clear queue */
        memset((void *) vq->queue, 0, size);

        /* setup queue */
        vq->desc  = (struct vring_desc *) vq->queue;
        vq->avail = (struct vring_avail *) (vq->queue + vq->num * sizeof(struct vring_desc));
        used_off = (uint32_t) vq->num * sizeof(struct vring_desc) + sizeof(uint16_t) * (2 + vq->num);
        used_off = (used_off + (VIRTIO_QUEUE_ALIGN - 1)) & ~(uint32_t) (VIRTIO_QUEUE_ALIGN - 1);
        vq->used = (struct vring_used *) (vq->queue + used_off);
        vq->last_used_idx = 0;

        /* activate queue */
        outl(vdev->io_base + VIRTIO_PCI_QUEUE_PFN, (uint32_t) (__pa(vq->queue) >> VIRTIO_QUEUE_SHIFT));

        /* add virtqueue to device */
        vq->vdev = vdev;
        list_add_tail(&vq->list, &vdev->vqs);

        return vq;
err:
        kfree(vq);
        return ERR_PTR(ret);
}

/*
 * Free a virtual queue.
 */
void free_vq(struct virtqueue *vq)
{
        list_del(&vq->list);
        if (vq->queue)
                free_pages(vq->queue, vq->queue_order);
        kfree(vq);
}

/*
 * Create a virtio device.
 */
struct virtio_device *create_virtio_device(struct pci_device *pci_dev)
{
        struct virtio_device *vdev;

        /* allocate a new virtio device */
        vdev = (struct virtio_device *) kmalloc(sizeof(struct virtio_device));
        if (!vdev)
                return NULL;

        /* clear device */
        memset(vdev, 0, sizeof(struct virtio_device));
        INIT_LIST_HEAD(&vdev->vqs);

        /* enable pci device */
        vdev->io_base = pci_dev->bar0 & ~(0x03);
        pci_enable_device(pci_dev);
        pci_set_master(pci_dev);

        /* init virtio device */
        vp_reset(vdev);
        vp_add_status(vdev, VIRTIO_STATUS_ACKNOWLEDGE);
        vp_add_status(vdev, VIRTIO_STATUS_DRIVER);
        vp_get_features(vdev);
        vp_set_features(vdev, 0);

        return vdev;
}

/*
 * Free a virtio device.
 */
void free_virtio_device(struct virtio_device *vdev)
{
        struct list_head *pos, *n;
        struct virtqueue *vq;

        /* free virtual queues */
        list_for_each_safe(pos, n, &vdev->vqs) {
                vq = list_entry(pos, struct virtqueue, list);
                free_vq(vq);
        }

        /* free device */
        kfree(vdev);
}

/*
 * Init virtio.
 */
int init_virtio()
{
	return init_virtio_rng();
}
