#ifndef _VIRTIO_H_
#define _VIRTIO_H_

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

int init_virtio();

/*
 * Compute vring size.
 */
static inline uint32_t vring_size(uint32_t num)
{
	return ((sizeof(struct vring_desc) * num + sizeof(uint16_t) * (2 + num)
		+ VIRTIO_QUEUE_ALIGN - 1) & ~(VIRTIO_QUEUE_ALIGN - 1))
		+ sizeof(uint16_t) * 2 + sizeof(struct vring_used_elem) * num;
}

#endif