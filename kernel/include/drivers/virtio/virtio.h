#ifndef _VIRTIO_H_
#define _VIRTIO_H_

#include <drivers/pci/pci.h>
#include <stddef.h>
#include <x86/io.h>

#define VIRTIO_PCI_VENDOR_ID		0x1AF4
#define VIRTIO_PCI_RNG_DEVICE_ID	0x1005

#define VIRTIO_PCI_HOST_FEATURES	0
#define VIRTIO_PCI_GUEST_FEATURES	4
#define VIRTIO_PCI_QUEUE_PFN		8
#define VIRTIO_PCI_QUEUE_SIZE		12
#define VIRTIO_PCI_QUEUE_SEL		14
#define VIRTIO_PCI_QUEUE_NOTIFY		16
#define VIRTIO_PCI_STATUS		18
#define VIRTIO_PCI_ISR			19

#define VIRTIO_STATUS_ACKNOWLEDGE	1
#define VIRTIO_STATUS_DRIVER		2
#define VIRTIO_STATUS_DRIVER_OK		4
#define VIRTIO_STATUS_FAILED		0x80

#define VRING_DESC_F_NEXT		1
#define VRING_DESC_F_WRITE		2

#define VIRTIO_QUEUE_ALIGN		4096
#define VIRTIO_QUEUE_SHIFT		12

struct vring_desc {
	uint64_t			addr;
	uint32_t			len;
	uint16_t			flags;
	uint16_t			next;
} __attribute__((packed));

struct vring_avail {
	uint16_t			flags;
	uint16_t			idx;
	uint16_t			ring[];
} __attribute__((packed));

struct vring_used_elem {
	uint32_t			id;
	uint32_t			len;
} __attribute__((packed));

struct vring_used {
	uint16_t			flags;
	uint16_t			idx;
	struct vring_used_elem		ring[];
} __attribute__((packed));

/*
 * Memory layout of a queue.
 */
struct vring {
	uint16_t			num;			/* number of entries in the queue */
	struct vring_desc *	 	desc;
	struct vring_avail *		avail;
	struct vring_used *		used;
} __attribute__((packed));

/*
 * Virtual queue.
 */
struct virtqueue {
	struct virtio_device *		vdev;			/* virtual device */
	int				index;			/* index of the queue */
	struct vring			vring;			/* memory layout of the queue */
	void *				queue;			/* virtual address of the ring queue */
	int				queue_order;		/* queue order */
	uint32_t			num_free;		/* number of free buffers */
	uint32_t			free_head;		/* head of free buffers list */
	uint32_t			num_added;		/* number we've added since last sync */
	uint16_t			last_used_idx;		/* last used index we've seen */
	struct list_head		list;
};

/*
 * Virtual I/O device.
 */
struct virtio_device {
	uint32_t			io_base;
	struct list_head 		vqs;
};

int init_virtio();
struct virtio_device *virtio_device_create(struct pci_device *pci_dev);
void virtio_device_free(struct virtio_device *vdev);
struct virtqueue *virtio_find_single_vq(struct virtio_device *vdev);
void virtqueue_kick(struct virtqueue *vq);
int virtqueue_add_buf(struct virtqueue *vq, void *buf, size_t len);
void virtqueue_get_buf(struct virtqueue *vq, size_t *len);

/*
 * Compute vring size.
 */
static inline uint32_t vring_size(uint32_t num)
{
	return ((sizeof(struct vring_desc) * num + sizeof(uint16_t) * (2 + num)
		+ VIRTIO_QUEUE_ALIGN - 1) & ~(VIRTIO_QUEUE_ALIGN - 1))
		+ sizeof(uint16_t) * 2 + sizeof(struct vring_used_elem) * num;
}

/*
 * Reset device.
 */
static inline void virtio_device_reset(struct virtio_device *vdev)
{
	outb(vdev->io_base + VIRTIO_PCI_STATUS, 0);
}

/*
 * Get device status.
 */
static inline uint8_t virtio_device_get_status(struct virtio_device *vdev)
{
	return inb(vdev->io_base + VIRTIO_PCI_STATUS);
}

/*
 * Set device status.
 */
static inline void virtio_device_set_status(struct virtio_device *vdev, uint8_t status)
{
	outb(vdev->io_base + VIRTIO_PCI_STATUS, status);
}

/*
 * Add to device status.
 */
static inline void virtio_device_add_status(struct virtio_device *vdev, uint8_t status)
{
	virtio_device_set_status(vdev, virtio_device_get_status(vdev) | status);
}

/*
 * Get device features.
 */
static inline uint32_t virtio_device_get_features(struct virtio_device *vdev)
{
	return inl(vdev->io_base + VIRTIO_PCI_HOST_FEATURES);
}

/*
 * Set device features.
 */
static inline void virtio_device_set_features(struct virtio_device *vdev, uint32_t features)
{
	outl(vdev->io_base + VIRTIO_PCI_GUEST_FEATURES, features);
}

#endif