#include <net/9p/9p.h>
#include <drivers/pci/pci.h>
#include <drivers/virtio/virtio.h>
#include <lib/scatterlist.h>
#include <mm/paging.h>
#include <fs/fs.h>
#include <stdio.h>
#include <stderr.h>

#define VIRTQUEUE_NUM		128

/*
 * Virtio 9p configuration.
 */
struct virtio_9p_config {
	uint16_t		tag_len;
	uint8_t			tag[0];
} __attribute__((packed));

/*
 * Virtio channel.
 */
struct virtio_chan {
	struct p9_client *	client;
	struct virtio_device *	vdev;
	struct virtqueue *	vq;
	struct scatterlist 	sg[VIRTQUEUE_NUM];
	int			tag_len;
	char *			tag;
	uint8_t			in_use;
	struct list_head	list;
};

/* channels */
static LIST_HEAD(virtio_chan_list);

/*
* Done request callback.
*/
static void req_done(struct virtqueue *vq)
{
	struct virtio_chan *chan = vq->vdev->priv;
	struct p9_request *req;
	size_t len;

	for (;;) {
		/* get request */
		req = virtqueue_get_buf(chan->vq, &len);
		if (!req)
			break;

		/* set request */
		req->rc.size = len;
		req->status = P9_REQUEST_STATUS_RCVD;
		p9_client_cb(req);
	}
}

/*
 * Create virtio transport.
 */
static int p9_virtio_create(struct p9_client *client, const char *addr, char *args)
{
	int ret = -ENOENT, found = 0;
	struct virtio_chan *chan;
	struct list_head *pos;

	/* unused arguments */
	UNUSED(args);

	/* find channel */
	list_for_each(pos, &virtio_chan_list) {
		chan = list_entry(pos, struct virtio_chan, list);
		if (strncmp(addr, chan->tag, chan->tag_len) == 0) {
			if (!chan->in_use) {
				chan->in_use = 1;
				found = 1;
				break;
			}
			ret = -EBUSY;
		}
	}

	/* no matching chennel */
	if (!found) {
		p9_error("p9_virtio_create: no matching channel \"%s\"\n", addr);
		return ret;
	}

	/* set channel */
	client->trans = chan;
	client->status = P9_CLIENT_CONNECTED;
	chan->client = client;

	return 0;
}

/*
 * Close virtio transport.
 */
static void p9_virtio_close(struct p9_client *client)
{
	struct virtio_chan *chan = client->trans;

	if (chan)
		chan->in_use = 0;
}

/*
 * Cancel a request.
 */
static int p9_virtio_cancel(struct p9_client *client, struct p9_request *req)
{
	UNUSED(client);
	UNUSED(req);
	return 1;
}

/*
 * How many bytes left in a page ?
 */
static uint32_t rest_of_page(void *data)
{
	return PAGE_SIZE - ((uint32_t) data % PAGE_SIZE);
}

/*
 * Pack a request into a scatter list.
 */
static int pack_sg_list(struct scatterlist *sg, int start, int limit, uint8_t *data, int count)
{
	int s, index = start;

	while (count) {
		s = rest_of_page(data);
		if (s > count)
			s = count;

		sg_set_buf(&sg[index++], data, s);

		count -= s;
		data += s;

		if (index > limit)
			panic("pack_sg_list: index > limit\n");
	}

	return index - start;
}

/*
 * Create a request.
 */
static int p9_virtio_request(struct p9_client *client, struct p9_request *req)
{
	struct virtio_chan *chan = client->trans;
	int out, in;

	/* pack request */
	out = pack_sg_list(chan->sg, 0, VIRTQUEUE_NUM, req->tc.sdata, req->tc.size);
	in = pack_sg_list(chan->sg, out, VIRTQUEUE_NUM, req->rc.sdata, req->rc.capacity);

	/* set request sent */
	req->status = P9_REQUEST_STATUS_SENT;

	/* add buffer */
	if (virtqueue_add_buf(chan->vq, chan->sg, out, in, req) < 0) {
		p9_error("p9_virtio_request: virtqueue_add_buf() failed\n");
		return -EIO;
	}

	/* kick queue */
	virtqueue_kick(chan->vq);

	return 0;
}

/*
 * 9p virtio transport.
 */
static struct p9_trans_module p9_virtio_trans = {
	.name 		= "virtio",
	.def		= 0,
	.maxsize 	= PAGE_SIZE * 16,
	.create 	= p9_virtio_create,
	.close 		= p9_virtio_close,
	.cancel		= p9_virtio_cancel,
	.request	= p9_virtio_request,
};

/*
* Probe a virtio 9p device.
*/
static int virtio_p9_probe(struct pci_device *pci_dev, struct pci_device_id *id)
{
	struct virtio_device *vdev;
	struct virtio_chan *chan;
	struct virtqueue *vq;
	int ret = -ENOMEM;
	uint16_t tag_len;
	char *tag;

	/* unused device id */
	UNUSED(id);

	/* allocate a new channel */
	chan = (struct virtio_chan *) kmalloc(sizeof(struct virtio_chan));
	if (!chan)
		goto err;
	memset(chan, 0, sizeof(struct virtio_chan));

	/* create a new virtio device */
	vdev = virtio_device_create(pci_dev);
	if (!vdev)
		goto err_free_chan;

	/* setup virtual queue */
	vq = virtio_find_single_vq(vdev, req_done);
	if (IS_ERR(vq)) {
		ret = PTR_ERR(vq);
		goto err_free_vdev;
	}

	/* driver ok */
	virtio_device_add_status(vdev, VIRTIO_STATUS_DRIVER_OK);

	/* get tag len */
	virtio_device_get_config(vdev, offsetof(struct virtio_9p_config, tag_len), &tag_len, sizeof(tag_len));

	/* get tag */
	tag = kmalloc(tag_len);
	if (!tag)
		goto err_free_vdev;
	virtio_device_get_config(vdev, offsetof(struct virtio_9p_config, tag), tag, tag_len);

	/* set channel */
	chan->vdev = vdev;
	chan->vq = vq;
	chan->tag_len = tag_len;
	chan->tag = tag;
	sg_init_table(chan->sg, VIRTQUEUE_NUM);
	vdev->priv = chan;

	/* add channel */
	list_add_tail(&chan->list, &virtio_chan_list);

	return 0;
err_free_vdev:
	virtio_device_add_status(vdev, VIRTIO_STATUS_FAILED);
	virtio_device_free(vdev);
err_free_chan:
	kfree(chan);
err:
	return ret;
}

/*
* PCI ids table.
*/
static struct pci_device_id virtio_p9_pci_tbl[] = {
	{ VIRTIO_PCI_VENDOR_ID, VIRTIO_PCI_P9_DEVICE_ID},
	{ 0, }
};

/*
* PCI driver.
*/
static struct pci_driver virtio_p9_pci_driver = {
	.id_table		= virtio_p9_pci_tbl,
	.probe			= virtio_p9_probe,
};

/*
 * Init virtio transport.
 */
int p9_trans_virtio_init()
{
	int ret;

	/* register virtio module */
	v9fs_register_trans(&p9_virtio_trans);

	/* register pci driver */
	ret = pci_register_driver(&virtio_p9_pci_driver);
	if (ret > 0)
		return 0;

	return ret == 0 ? -ENODEV : ret;
}