#include <drivers/virtio/virtio.h>
#include <drivers/virtio/virtio_rng.h>
#include <mm/paging.h>
#include <fs/fs.h>
#include <string.h>
#include <stderr.h>

/*
 * Detach a buffer.
 */
static void detach_buf(struct virtqueue *vq, uint32_t head)
{
        uint32_t i;

	/* put back on free list */
        for (i = head; vq->vring.desc[i].flags & VRING_DESC_F_NEXT;) {
		i = vq->vring.desc[i].next;
		vq->num_free++;
	}

	/* plus final descriptor */
	vq->vring.desc[i].next = vq->free_head;
	vq->free_head = head;
	vq->num_free++;
}

/*
 * Get a buffer.
 */
void virtqueue_get_buf(struct virtqueue *vq, size_t *len)
{
        size_t i;

        /* get buffer */
        i = vq->vring.used->ring[vq->last_used_idx % vq->vring.num].id;
	*len = vq->vring.used->ring[vq->last_used_idx % vq->vring.num].len;

        /* detach buffer */
        detach_buf(vq, i);
	vq->last_used_idx++;
}

/*
 * Add a buffer.
 */
int virtqueue_add_buf(struct virtqueue *vq, void *buf, size_t len)
{
        struct vring *vr = &vq->vring;
        int head, avail;

        /* no free buffer */
        if (vq->num_free < 1)
                return -ENOSPC;

        /* set descriptor */
        head = vq->free_head;
        vr->desc[head].addr = __pa(buf);
        vr->desc[head].len = len;
        vr->desc[head].flags = VRING_DESC_F_WRITE;

        /* update free pointer */
        vq->num_free--;
        vq->free_head = head + vr->desc[head].next;

        /* publish descriptor */
        avail = (vq->vring.avail->idx + vq->num_added++) % vq->vring.num;
	vq->vring.avail->ring[avail] = head;

        return 0;
}

/*
 * Kick a virtual queue.
 */
void virtqueue_kick(struct virtqueue *vq)
{
        struct vring *vr = &vq->vring;

        /* descriptors and available array need to be set before we expose the new available array entries */
        __asm__ volatile("" ::: "memory");
        vr->avail->idx += vq->num_added;
        vq->num_added = 0;
        __asm__ volatile("" ::: "memory");

        /* kick queue */
        outw(vq->vdev->io_base + VIRTIO_PCI_QUEUE_NOTIFY, vq->index);
}

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
        size_t size, i;

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

        /* put everything in free lists */
	vq->num_free = num;
	vq->free_head = 0;
	for (i = 0; i < num - 1; i++)
		vq->vring.desc[i].next = i + 1;

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
