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
static struct virtqueue *vq = NULL;
static uint32_t *random_data = NULL;

/*
 * Read a random buffer.
 */
static int virtio_rng_read_buf(size_t len)
{
        struct vring *vr = &vq->vring;
        int done = 0, ret, i;
        size_t n;

        /* limit length to random data size */
        if (len > RANDOM_DATA_SIZE)
                len = RANDOM_DATA_SIZE;

        /* add buffer */
        ret = virtqueue_add_buf(vq, random_data, len);
        if (ret)
                return ret;

        /* kick queue */
        virtqueue_kick(vq);

        /* poll the device */
        for (i = 0; i < 100000000; i++) {
                if (vr->used->idx != vq->last_used_idx) {
                        done = 1;
                        break;
                }

                __asm__ volatile("pause");
        }

        /* timeout */
        if (!done)
                return -EIO;

        /* get buffer */
        virtqueue_get_buf(vq, &n);
        if (n > len)
                n = len;

        /* ack pending irq */
        inb(vq->vdev->io_base + VIRTIO_PCI_ISR);

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
        if (!buf || !len || !vq)
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
* Probe a virtio random generator.
*/
static int virtio_rng_probe(struct pci_device *pci_dev, struct pci_device_id *id)
{
        struct virtio_device *vdev;
        int ret;

	/* unused device id */
	UNUSED(id);

        /* device already set up */
        if (vq)
                return -EBUSY;

        /* create a new virtio device */
        vdev = virtio_device_create(pci_dev);
        if (!vdev)
                return -ENOMEM;

        /* setup virtual queue */
        vq = virtio_find_single_vq(vdev);
        if (IS_ERR(vq)) {
                ret = PTR_ERR(vq);
                goto err;
        }

        /* driver ok */
        virtio_device_add_status(vdev, VIRTIO_STATUS_DRIVER_OK);

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
        virtio_device_add_status(vdev, VIRTIO_STATUS_FAILED);
        virtio_device_free(vdev);
        vq = NULL;
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