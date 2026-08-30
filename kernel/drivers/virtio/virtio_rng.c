#include <drivers/virtio/virtio_rng.h>
#include <drivers/virtio/virtio.h>
#include <drivers/pci/pci.h>
#include <drivers/char/misc.h>
#include <mm/paging.h>
#include <string.h>
#include <stderr.h>
#include <stdio.h>

#define RANDOM_DATA_SIZE        64

/* virtio rng device */
struct virtio_device *vdev_rng = NULL;
static uint32_t *random_data = NULL;

/*
 * Read a random buffer.
 */
static int virtio_rng_read_buf(struct virtio_device *vdev, size_t len)
{
        int done = 0, i;
        size_t n;

        /* limit length to random data size */
        if (len > RANDOM_DATA_SIZE)
                len = RANDOM_DATA_SIZE;

        /* set descriptor */
        vdev->vq.desc[0].addr = __pa(random_data);
        vdev->vq.desc[0].len = len;
        vdev->vq.desc[0].flags = VRING_DESC_F_WRITE;
        vdev->vq.desc[0].next = 0;

        /* publish descriptor 0 */
        vdev->vq.avail->ring[vdev->vq.avail->idx % vdev->vq.num] = 0;
        __asm__ volatile("" ::: "memory");
        vdev->vq.avail->idx++;
        __asm__ volatile("" ::: "memory");

        /* kick queue 0 */
        outw(vdev->io_base + VIRTIO_PCI_QUEUE_NOTIFY, 0);

        /* poll the device */
        for (i = 0; i < 100000000; i++) {
                if (vdev->vq.used->idx != vdev->vq.last_used_idx) {
                        done = 1;
                        break;
                }

                __asm__ volatile("pause");
        }

        /* timeout */
        if (!done)
                return -EIO;

        /* get read length */
        n = vdev->vq.used->ring[vdev->vq.last_used_idx % vdev->vq.num].len;
        vdev->vq.last_used_idx++;
        if (n > len)
                n = len;

        /* ack pending irq */
        inb(vdev->io_base + VIRTIO_PCI_ISR);

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
        if (!random_data || !buf || !len || !vdev_rng || vdev_rng->vq.num < 1)
                return -EINVAL;

        while (len > 0) {
                /* read random buffer */
                ret = virtio_rng_read_buf(vdev_rng, len);
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
* Probe a virtio random generator.
*/
static int virtio_rng_probe(struct pci_device *pci_dev, struct pci_device_id *id)
{
        int ret;

	/* unused device id */
	UNUSED(id);

        /* device already set up */
        if (vdev_rng)
                return -EBUSY;

        /* allocate a new virtio device */
        vdev_rng = (struct virtio_device *) kmalloc(sizeof(struct virtio_device));
        if (!vdev_rng)
                return -ENOMEM;
        memset(vdev_rng, 0, sizeof(struct virtio_device));

        /* enable pci device */
        vdev_rng->io_base = pci_dev->bar0 & ~(0x03);
        pci_enable_device(pci_dev);
        pci_set_master(pci_dev);

        /* init virtio device */
        vp_reset(vdev_rng);
        vp_add_status(vdev_rng, VIRTIO_STATUS_ACKNOWLEDGE);
        vp_add_status(vdev_rng, VIRTIO_STATUS_DRIVER);
        vp_get_features(vdev_rng);
        vp_set_features(vdev_rng, 0);

        /* setup virtual queue */
        ret = setup_vq(vdev_rng);
        if (ret)
                goto err;

        /* driver ok */
        vp_add_status(vdev_rng, VIRTIO_STATUS_DRIVER_OK);

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
        vp_add_status(vdev_rng, VIRTIO_STATUS_FAILED);
        kfree(vdev_rng);
        vdev_rng = NULL;
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
        int ret;

        /* register pci driver */
	ret = pci_register_driver(&virtio_rng_pci_driver);
        if (ret > 0)
                return 0;

        return ret == 0 ? -ENODEV : ret;
}