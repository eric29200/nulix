#include <drivers/virtio/virtio.h>
#include <drivers/virtio/virtio_rng.h>
#include <mm/paging.h>
#include <fs/fs.h>
#include <string.h>
#include <stdio.h>
#include <stderr.h>

/* virtio device id */
static int virtio_device_id = 0;

/*
* Detach a buffer.
*/
static void detach_buf(struct virtqueue *vq, uint32_t head)
{
	uint32_t i;

	/* clear data */
	vq->data[head] = NULL;

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
void *virtqueue_get_buf(struct virtqueue *vq, size_t *len)
{
	void *ret;
	size_t i;

	/* no buffers in queue */
	if (vq->last_used_idx == vq->vring.used->idx)
		return NULL;

	/* get buffer */
	i = vq->vring.used->ring[vq->last_used_idx % vq->vring.num].id;
	*len = vq->vring.used->ring[vq->last_used_idx % vq->vring.num].len;

	/* detach buffer */
	ret = vq->data[i];
	detach_buf(vq, i);
	vq->last_used_idx++;

	return ret;
}

/*
* Add a buffer.
*/
int virtqueue_add_buf(struct virtqueue *vq, struct scatterlist sg[], size_t out_num, size_t in_num, void *data)
{
	struct vring *vr = &vq->vring;
	uint32_t prev = 0, i;
	int head, avail;

	/* check counts */
	if (out_num + in_num > vr->num)
		panic("virtqueue_add_buf: out_num + in_num > vr->num\n");
	if (out_num + in_num == 0)
		panic("virtqueue_add_buf: out_num and in_num = 0\n");

	/* no free buffer */
	if (vq->num_free < out_num + in_num)
		return -ENOSPC;

	/* we're about to use some buffers from the free list */
	vq->num_free -= out_num + in_num;

	/* set output descriptors */
	head = vq->free_head;
	for (i = vq->free_head; out_num != 0; i = vq->vring.desc[i].next, out_num--) {
		vq->vring.desc[i].flags = VRING_DESC_F_NEXT;
		vq->vring.desc[i].addr = sg_phys(sg);
		vq->vring.desc[i].len = sg->length;
		prev = i;
		sg++;
	}

	/* set input descriptors */
	for (; in_num != 0; i = vq->vring.desc[i].next, in_num--) {
		vq->vring.desc[i].flags = VRING_DESC_F_NEXT | VRING_DESC_F_WRITE;
		vq->vring.desc[i].addr = sg_phys(sg);
		vq->vring.desc[i].len = sg->length;
		prev = i;
		sg++;
	}

	/* last one doesn't continue */
	vq->vring.desc[prev].flags &= ~VRING_DESC_F_NEXT;

	/* update free pointer */
	vq->free_head = i;

	/* set data */
	vq->data[head] = data;

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
static struct virtqueue *vring_new_virtqueue(struct virtio_device *vdev, int index, uint32_t num, vq_callback_t *callback)
{
	struct virtqueue *vq;
	size_t size, i;

	/* allocate a new virt queue */
	vq = (struct virtqueue *) kmalloc(sizeof(struct virtqueue) + num * sizeof(void *));
	if (!vq)
		return NULL;

	/* clear virtual queue */
	memset(vq, 0, sizeof(struct virtqueue));
	vq->callback = callback;

	/* compute size of virtual queue */
	size = PAGE_ALIGN_UP(vring_size(num));
	vq->queue_order = get_order(size);

	/* allocate queue */
	vq->queue = get_free_pages(vq->queue_order);
	if (!vq->queue) {
		kfree(vq);
		return NULL;
	}

	/* clear memory */
	memset(vq->queue, 0, size);

	/* init memory layout */
	vring_init(&vq->vring, num, vq->queue);

	/* put everything in free lists */
	vq->num_free = num;
	vq->free_head = 0;
	for (i = 0; i < num - 1; i++)
	vq->vring.desc[i].next = i + 1;
	vq->data[i] = NULL;

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
static struct virtqueue *setup_vq(struct virtio_device *vdev, int index, vq_callback_t *callback)
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
	vq = vring_new_virtqueue(vdev, index, num, callback);
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

	/* free irq */
	if (vdev->irq_enabled) {
		free_irq(vdev->pci_dev->irq, vdev);
		vdev->irq_enabled = 0;
	}

	/* free virtual queues */
	list_for_each_safe(pos, n, &vdev->vqs) {
		vq = list_entry(pos, struct virtqueue, list);
		virtio_del_vq(vq);
	}
}

/*
* Virtual queue irq interrupt.
*/
static void vring_interrupt(struct virtqueue *vq)
{
	if (vq->last_used_idx == vq->vring.used->idx) {
		printf("vring_interrupt: no work for virtual queue\n");
		return;
	}

	if (vq->callback)
		vq->callback(vq);
}

/*
* Virtio IRQ handler.
*/
static void virtio_irq_handler(struct registers *regs, void *dev_instance)
{
	struct virtio_device *vdev = dev_instance;
	struct list_head *pos;
	struct virtqueue *vq;
	uint8_t isr;

	/* unused registers */
	UNUSED(regs);

	/* read isr */
	isr = inb(vdev->io_base + VIRTIO_PCI_ISR);
	if (!isr)
		return;

	/* handle virtqueues */
	list_for_each(pos, &vdev->vqs) {
		vq = list_entry(pos, struct virtqueue, list);
		vring_interrupt(vq);
	}
}

/*
* Request irq.
*/
static int virtio_request_irq(struct virtio_device *vdev)
{
	int ret;

	ret = request_irq(vdev->pci_dev->irq, virtio_irq_handler, SA_SHIRQ, vdev->name, vdev);
	if (ret == 0)
		vdev->irq_enabled = 1;

	return ret;
}

/*
* Find virtqueues.
*/
static int virtio_find_vqs(struct virtio_device *vdev, size_t nvqs, struct virtqueue *vqs[], vq_callback_t *callbacks[])
{
	size_t i;
	int ret;

	/* request irq */
	ret = virtio_request_irq(vdev);
	if (ret)
		return ret;

	/* set up virt queues */
	for (i = 0; i < nvqs; i++) {
		vqs[i] = setup_vq(vdev, i, callbacks[i]);
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
struct virtqueue *virtio_find_single_vq(struct virtio_device *vdev, vq_callback_t *callback)
{
	vq_callback_t *callbacks[] = { callback };
	struct virtqueue *vq;
	int ret;

	ret = virtio_find_vqs(vdev, 1, &vq, callbacks);
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
	vdev->pci_dev = pci_dev;
	INIT_LIST_HEAD(&vdev->vqs);

	/* set name */
	snprintf(vdev->name, VIRTIO_DEV_NAME_LEN, "virtio%d\n", virtio_device_id++);

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
