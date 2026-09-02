#include <drivers/virtio/virtio.h>
#include <drivers/virtio/virtio_rng.h>
#include <mm/paging.h>
#include <fs/fs.h>
#include <string.h>
#include <stderr.h>

/*
 * Init memory layout of a queue.
 */
static void vring_init(struct vring *vr, uint32_t num, void *p)
{
	vr->num = num;
	vr->desc = p;
	vr->avail = p + num * sizeof(struct vring_desc);
        vr->used = (void *)(((uint32_t) &vr->avail->ring[num] + VIRTIO_QUEUE_ALIGN - 1) & ~(VIRTIO_QUEUE_ALIGN - 1));
}

/*
 * Create a new virtual queue.
 */
static struct virtqueue *vring_new_virtqueue(struct virtio_device *vdev, int index, uint32_t num)
{
        struct virtqueue *vq;
        size_t size;

        /* allocate a new virt queue */
        vq = (struct virtqueue *) kmalloc(sizeof(struct virtqueue));
        if (!vq)
                return NULL;

        /* clear virtual queue */
        memset(vq, 0, sizeof(struct virtqueue));

        /* compute size of virtual queue */
        size = PAGE_ALIGN_UP(vring_size(num));
        vq->queue_order = get_order(size);

        /* allocate queue */
        vq->queue = get_free_pages(vq->queue_order);
        if (!vq->queue) {
                kfree(vq);
                return NULL;
        }

        /* init memory layout */
        vring_init(&vq->vring, num, vq->queue);

        /* activate queue */
        outl(vdev->io_base + VIRTIO_PCI_QUEUE_PFN, (uint32_t) (__pa(vq->queue) >> VIRTIO_QUEUE_SHIFT));

        /* add virtqueue to device */
        vq->index = index;
        vq->vdev = vdev;
        list_add_tail(&vq->list, &vdev->vqs);

        return vq;
}

/*
 * Setup virtual queue.
 */
static struct virtqueue *setup_vq(struct virtio_device *vdev, int index)
{
        struct virtqueue *vq;
        uint16_t num;

        /* select queue */
        outw(vdev->io_base + VIRTIO_PCI_QUEUE_SEL, index);

        /* check if queue is either not available or already active */
        num = inw(vdev->io_base + VIRTIO_PCI_QUEUE_SIZE);
        if (!num || inl(vdev->io_base + VIRTIO_PCI_QUEUE_PFN))
                return ERR_PTR(-ENOENT);

        /* fix queue size */
        if (num > 256)
                num = 256;

        /* create virtual queue */
        vq = vring_new_virtqueue(vdev, index, num);
        if (!vq)
                return ERR_PTR(-ENOMEM);

        return vq;
}

/*
 * Delete a virtual queue.
 */
static void virtio_del_vq(struct virtqueue *vq)
{
        list_del(&vq->list);
        if (vq->queue)
                free_pages(vq->queue, vq->queue_order);
        kfree(vq);
}

/*
 * Delete virtual queue of a device.
 */
static void virtio_del_vqs(struct virtio_device *vdev)
{
        struct list_head *pos, *n;
        struct virtqueue *vq;

        /* free virtual queues */
        list_for_each_safe(pos, n, &vdev->vqs) {
                vq = list_entry(pos, struct virtqueue, list);
                virtio_del_vq(vq);
        }
}

/*
 * Find virtqueues.
 */
static int virtio_find_vqs(struct virtio_device *vdev, size_t nvqs, struct virtqueue *vqs[])
{
        size_t i;
        int ret;

        /* set up virt queues */
        for (i = 0; i < nvqs; i++) {
                vqs[i] = setup_vq(vdev, i);
                if (IS_ERR(vqs[i])) {
                        ret = PTR_ERR(vqs[i]);
                        goto err;
                }
        }

        return 0;
err:
        virtio_del_vqs(vdev);
        return ret;
}

/*
 * Find a single virtual queue.
 */
struct virtqueue *virtio_find_single_vq(struct virtio_device *vdev)
{
	struct virtqueue *vq;
        int ret;

        ret = virtio_find_vqs(vdev, 1, &vq);
        if (ret)
		return ERR_PTR(ret);

	return vq;
}

/*
 * Create a virtio device.
 */
struct virtio_device *virtio_device_create(struct pci_device *pci_dev)
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
        virtio_device_reset(vdev);
        virtio_device_add_status(vdev, VIRTIO_STATUS_ACKNOWLEDGE);
        virtio_device_add_status(vdev, VIRTIO_STATUS_DRIVER);
        virtio_device_get_features(vdev);
        virtio_device_set_features(vdev, 0);

        return vdev;
}

/*
 * Free a virtio device.
 */
void virtio_device_free(struct virtio_device *vdev)
{
        virtio_del_vqs(vdev);
        kfree(vdev);
}

/*
 * Init virtio.
 */
int init_virtio()
{
	return init_virtio_rng();
}
