#include <drivers/virtio/virtio_rng.h>
#include <drivers/virtio/virtio.h>
#include <drivers/pci/pci.h>
#include <drivers/char/misc.h>
#include <mm/paging.h>
#include <string.h>
#include <stderr.h>
#include <stdio.h>

#define RANDOM_DATA_SIZE        64

/*
 * Virtio rng driver.
 */
struct virtio_rng {
        int                     present;                /* driver present ? */
        uint16_t                io_base;                /* pci I/O address base */
        uint16_t                num;                    /* number of entries in the queue */
        struct vring_desc *     desc;                   /* memory layout of the queue */
        struct vring_avail *    avail;
        struct vring_used  *    used;
        void *                  queue;                  /* virtual address of the ring queue */
        uint16_t                last_used_idx;          /* last used index we've seen */
};

/* virtio rng driver */
static struct virtio_rng vr = { 0 };
static uint32_t *random_data;

/*
 * Reset device.
 */
static inline void vp_reset()
{
        outb(vr.io_base + VIRTIO_PCI_STATUS, 0);
}

/*
 * Get device status.
 */
static inline uint8_t vp_get_status()
{
	return inb(vr.io_base + VIRTIO_PCI_STATUS);
}

/*
 * Set device status.
 */
static inline void vp_set_status(uint8_t status)
{
	outb(vr.io_base + VIRTIO_PCI_STATUS, status);
}

/*
 * Add to device status.
 */
static inline void vp_add_status(uint8_t status)
{
        vp_set_status(vp_get_status() | status);
}

/*
 * Get device features.
 */
static inline uint32_t vp_get_features()
{
	return inl(vr.io_base + VIRTIO_PCI_HOST_FEATURES);
}

/*
 * Set device features.
 */
static inline void vp_set_features(uint32_t features)
{
        outl(vr.io_base + VIRTIO_PCI_GUEST_FEATURES, features);
}

/*
 * Read a random buffer.
 */
static int virtio_rng_read_buf(size_t len)
{
        int done = 0, i;
        size_t n;

        /* limit length to random data size */
        if (len > RANDOM_DATA_SIZE)
                len = RANDOM_DATA_SIZE;

        /* set descriptor */
        vr.desc[0].addr = __pa(random_data);
        vr.desc[0].len = len;
        vr.desc[0].flags = VRING_DESC_F_WRITE;
        vr.desc[0].next = 0;

        /* publish descriptor 0 */
        vr.avail->ring[vr.avail->idx % vr.num] = 0;
        __asm__ volatile("" ::: "memory");
        vr.avail->idx++;
        __asm__ volatile("" ::: "memory");

        /* kick queue 0 */
        outw(vr.io_base + VIRTIO_PCI_QUEUE_NOTIFY, 0);

        /* poll the device */
        for (i = 0; i < 100000000; i++) {
                if (vr.used->idx != vr.last_used_idx) {
                        done = 1;
                        break;
                }

                __asm__ volatile("pause");
        }

        /* timeout */
        if (!done)
                return -EIO;

        /* get read length */
        n = vr.used->ring[vr.last_used_idx % vr.num].len;
        vr.last_used_idx++;
        if (n > len)
                n = len;

        /* ack pending irq */
        inb(vr.io_base + VIRTIO_PCI_ISR);

        return n;
}

/*
 * Read from rng virtio device.
 */
static int virtio_rng_read(struct file *filp, char *buf, size_t len, off_t *off)
{
        int ret = 0, n = 0;

        /* unused parameters */
        UNUSED(filp);
        UNUSED(off);

        /* check parameters */
        if (!vr.present || !buf || !len || vr.num < 1)
                return -EINVAL;

        while (len > 0) {
                /* read random buffer */
                ret = virtio_rng_read_buf(len);
                if (ret <= 0)
                        break;

                /* copy to user buffer */
                memcpy(buf, random_data, ret);

                /* go to next buffer */
                buf += ret;
                len -= ret;
                n += ret;
        }

        return n > 0 ? n : ret;
}

/*
 * Virtio rng file operations.
 */
static struct file_operations virtio_rng_fops = {
        .read           = virtio_rng_read,
};

/*
 * Virtio rng misc device.
 */
static struct misc_device virtio_rng_misc_dev = {
        .name           = "virtio-rng",
        .minor          = DEV_MINOR_VIRTIO_RNG,
        .fops           = &virtio_rng_fops,
};

/*
 * Setup virtual queue.
 */
static int setup_vq()
{
        size_t size, used_off;
        int order;

        /* select queue 0 */
        outw(vr.io_base + VIRTIO_PCI_QUEUE_SEL, 0);

        /* check if queue is either not available or already active */
        vr.num = inw(vr.io_base + VIRTIO_PCI_QUEUE_SIZE);
        if (!vr.num || inl(vr.io_base + VIRTIO_PCI_QUEUE_PFN))
                return -ENOENT;

        /* fix queue size */
        if (vr.num > 256)
                vr.num = 256;

        /* compute size of virtual queue */
        size = PAGE_ALIGN_UP(vring_size(vr.num));
        order = get_order(size);

        /* allocate queue */
        vr.queue = get_free_pages(order);
        if (!vr.queue)
                return -ENOMEM;

        /* clear queue */
        memset((void *) vr.queue, 0, size);

        /* setup queue */
        vr.desc  = (struct vring_desc *) vr.queue;
        vr.avail = (struct vring_avail *) (vr.queue + vr.num * sizeof(struct vring_desc));
        used_off = (uint32_t) vr.num * sizeof(struct vring_desc) + sizeof(uint16_t) * (2 + vr.num);
        used_off = (used_off + (VIRTIO_QUEUE_ALIGN - 1)) & ~(uint32_t) (VIRTIO_QUEUE_ALIGN - 1);
        vr.used  = (struct vring_used *) (vr.queue + used_off);
        vr.last_used_idx = 0;

        /* activate queue */
        outl(vr.io_base + VIRTIO_PCI_QUEUE_PFN, (uint32_t) (__pa(vr.queue) >> VIRTIO_QUEUE_SHIFT));

        return 0;
}

/*
* Probe a virtio random generator.
*/
static int virtio_rng_probe(struct pci_device *pci_dev, struct pci_device_id *id)
{
        int ret;

	/* unused device id */
	UNUSED(id);

        /* enable pci device */
        vr.io_base = pci_dev->bar0 & ~(0x03);
        pci_enable_device(pci_dev);
        pci_set_master(pci_dev);

        /* init virtio device */
        vp_reset();
        vp_add_status(VIRTIO_STATUS_ACKNOWLEDGE);
        vp_add_status(VIRTIO_STATUS_DRIVER);
        vp_get_features();
        vp_set_features(0);

        /* setup virtual queue */
        ret = setup_vq();
        if (ret)
                goto err;

        /* driver ok */
        vp_add_status(VIRTIO_STATUS_DRIVER_OK);
        vr.present = 1;

        /* register misc device */
        ret = misc_register(&virtio_rng_misc_dev);
        if (ret)
                goto err;

        /* allocate random buffer */
        random_data = kmalloc(RANDOM_DATA_SIZE);
        if (!random_data) {
                ret = -ENOMEM;
                goto err;
        }

        return 0;
err:
        vp_add_status(VIRTIO_STATUS_FAILED);
        return ret;
}

/*
 * PCI ids table.
 */
static struct pci_device_id virtio_rng_pci_tbl[] = {
        { VIRTIO_PCI_VENDOR_ID, VIRTIO_PCI_RNG_DEVICE_ID},
        { 0, }
};

/*
 * PCI driver.
 */
static struct pci_driver virtio_rng_pci_driver = {
	.id_table		= virtio_rng_pci_tbl,
	.probe			= virtio_rng_probe,
};

/*
* Init virtio random generator.
*/
int init_virtio_rng()
{
	return pci_register_driver(&virtio_rng_pci_driver);
}