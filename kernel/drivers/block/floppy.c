#include <drivers/block/floppy.h>
#include <drivers/block/blk_dev.h>
#include <mm/highmem.h>
#include <mm/paging.h>
#include <x86/cmos.h>
#include <x86/io.h>
#include <string.h>
#include <stderr.h>
#include <stdio.h>

#define FLOPPY_TRACKS			80
#define FLOPPY_SECTORS_PER_TRACK	18
#define FLOPPY_HEADS			2

#define FDC_DOR				0x3F2
#define FDC_MSR				0x3F4
#define FDC_FIFO			0x3F5

#define DMA_CH2_ADDR			0x04
#define DMA_CH2_COUNT			0x05
#define DMA_PAGE_2			0x81
#define DMA_MASK_REG			0x0A
#define DMA_MODE_REG			0x0B
#define DMA_CLEAR_FF			0x0C


/* global variables */
static size_t floppy_sizes[256];
static size_t floppy_blksizes[256];
static void *dma_base = NULL;

/*
 * Wait for floppy device.
 */
static void floppy_wait()
{
	int i;

	for (i = 0; i < 200000; i++);
}

/*
 * Send a command.
 */
static void fdc_send_cmd(uint8_t cmd)
{
	/* wait for MSR */
	while ((inb(FDC_MSR) & 0xC0) != 0x80);

	/* send command */
	outb(FDC_FIFO, cmd);
}

/*
 * Read data.
 */
static uint8_t fdc_read_data()
{
	/* wait for MSR */
	while ((inb(FDC_MSR) & 0xC0) != 0xC0);

	/* read data */
	return inb(FDC_FIFO);
}

/*
 * Ack FDC.
 */
static void floppy_sense_interrupt(uint8_t *st0, uint8_t *pcn)
{
	fdc_send_cmd(0x08);
	*st0 = fdc_read_data();
	*pcn = fdc_read_data();
}

/*
 * Turn motor on.
 */
static void floppy_motor_on(int drive)
{
	outb(FDC_DOR, 0x0C | (drive & 0x03) | (0x10 << drive));
	floppy_wait();
}

/*
 * Tur motor off.
 */
static void floppy_motor_off(int drive)
{
	outb(FDC_DOR, 0x0C | (drive & 0x03));
}

/*
 * Setup dma.
 */
static void floppy_setup_dma(size_t length, int write)
{
	/* mask dma channel */
	outb(DMA_MASK_REG, 0x06);

	/* load dma adress */
	outb(DMA_CLEAR_FF, 0xFF);
	outb(DMA_CH2_ADDR, (uint8_t) (__pa(dma_base) & 0xFF));
	outb(DMA_CH2_ADDR, (uint8_t) ((__pa(dma_base) >> 8) & 0xFF));
	outb(DMA_PAGE_2, (uint8_t)((__pa(dma_base) >> 16) & 0xFF));
	outb(DMA_CLEAR_FF, 0xFF);

	/* load count - 1 */
	outb(DMA_CH2_COUNT, (uint8_t) ((length - 1) & 0xFF));
	outb(DMA_CH2_COUNT, (uint8_t) (((length - 1) >> 8) & 0xFF));

	/* set mode */
	outb(DMA_MODE_REG, write ? 0x5A : 0x56);

	/* activate dma channel */
	outb(DMA_MASK_REG, 0x02);
}

/*
 * Read/write a sector.
 */
static void fd_do_rw_sector(int drive, struct request *req)
{
	uint8_t track, head, sector;
	void *buf;
	int i;

	/* get track/head/sector */
	track = req->sector / (FLOPPY_SECTORS_PER_TRACK * FLOPPY_HEADS);
	head = (req->sector / FLOPPY_SECTORS_PER_TRACK) % FLOPPY_HEADS;
	sector = (req->sector % FLOPPY_SECTORS_PER_TRACK) + 1;

	/* write request : copy buffer to dma */
	if (req->cmd == WRITE) {
		buf = bh_kmap(req->bh) + req->bh_offset;
		memcpy(dma_base, buf, 512);
		bh_kunmap(req->bh);
	}

	/* configure dma */
	floppy_setup_dma(512, req->cmd == WRITE ? 1 : 0);

	/* send read/write */
	fdc_send_cmd(req->cmd == READ ? 0x46 : 0x45);
	fdc_send_cmd((head << 2) | (drive & 0x03));
	fdc_send_cmd(track);
	fdc_send_cmd(head);
	fdc_send_cmd(sector);
	fdc_send_cmd(0x02);
	fdc_send_cmd(FLOPPY_SECTORS_PER_TRACK);
	fdc_send_cmd(0x1B);
	fdc_send_cmd(0xFF);

	/* clear fdc status */
	for (i = 0; i < 7; i++)
		fdc_read_data();

	/* read request : copy buffer from dma */
	if (req->cmd == READ) {
		buf = bh_kmap(req->bh) + req->bh_offset;
		memcpy(buf, dma_base, 512);
		bh_kunmap(req->bh);
	}
}

/*
 * Do read/write.
 */
static int fd_do_rw(struct request *req)
{
	int drive = minor(req->rq_dev);

	/* turn motor on */
	floppy_motor_on(drive);

	while (req->nr_sectors > 0) {
		/* read/write sector */
		fd_do_rw_sector(drive, req);

		/* update request */
		req->sector++;
		req->nr_sectors--;
		req->current_nr_sectors--;
		req->bh_offset += 512;

		/* go to next buffer */
		if (req->bh_offset >= req->bh->b_size) {
			req->bh_offset = 0;
			req->bh = list_next_entry_or_null(req->bh, &req->bhs_list, b_list_req);
		}
	}

	/* end request */
	if (req->current_nr_sectors == 0)
		end_request(req, 1);

	/* turn motor off */
	floppy_motor_off(drive);

	return 0;
}

/*
 * Handle read/write request.
 */
static void fd_request()
{
	struct request *request;

	for (;;) {
		/* get next request */
		request = blk_dev[DEV_FLOPPY_MAJOR].current_request;
		if (!request)
			return;

		/* remove it from queue */
		blk_dev[DEV_FLOPPY_MAJOR].current_request = request->next;

		/* do request */
		fd_do_rw(request);
	}
}

/*
 * Open a floppy device.
 */
static int fd_open(struct inode *inode, struct file *filp)
{
	int minor = minor(inode->i_rdev);

	/* unused file */
	UNUSED(filp);

	/* check minor number */
	if (minor != 0 && minor != 1)
		return -ENXIO;

	return 0;
}

/*
 * Close a floppy device.
 */
static int fd_release(struct inode *inode, struct file *filp)
{
	int minor = minor(inode->i_rdev);

	/* unused file */
	UNUSED(filp);

	/* check minor number */
	if (minor != 0 && minor != 1)
		return -ENXIO;

	return 0;
}

/*
 * Floppy ioctl.
 */
static int fd_ioctl(struct inode *inode, struct file *filp, int request, unsigned long arg)
{
	dev_t dev = inode->i_rdev;

	UNUSED(filp);

	switch (request) {
		case BLKGETSIZE:
			*((uint32_t *) arg) = FLOPPY_TRACKS * FLOPPY_SECTORS_PER_TRACK * FLOPPY_HEADS;
			break;
		case BLKGETSIZE64:
			*((uint64_t *) arg) = FLOPPY_TRACKS * FLOPPY_SECTORS_PER_TRACK * FLOPPY_HEADS * 512;
			break;
		case BLKDISCARDZEROES:
			break;
		case BLKROGET:
		case BLKBSZGET:
		case BLKBSZSET:
		case BLKSSZGET:
			return blk_ioctl(inode->i_rdev, request, arg);
		default:
			printk("Unknown ioctl request (0x%x) on device 0x%x\n", request, (int) dev);
			break;
	}

	return 0;
}

/*
 * Floppy file operations.
 */
static struct file_operations floppy_fops = {
	.open		= fd_open,
	.release	= fd_release,
	.read		= generic_block_read,
	.write		= generic_block_write,
	.ioctl		= fd_ioctl,
};

/*
 * Init a floppy drive.
 */
static void init_floppy_drive(int drive)
{
	uint8_t st0, pcn;

	/* select drive */
	outb(FDC_DOR, 0x0C | drive | (0x10 << drive));
	floppy_wait();

	/* RECALIBRATE drive */
	fdc_send_cmd(0x07);
	fdc_send_cmd(drive);

	/* ack FDC */
	floppy_sense_interrupt(&st0, &pcn);
	floppy_wait();

	/* turn off motor */
	outb(FDC_DOR, 0x0C);
}

/*
 * Init floppy devices.
 */
int init_floppy()
{
	uint8_t cmos_floppy;
	int ret;

	/* get dma memory */
	dma_base = get_free_page();
	if (!dma_base)
		return -ENOMEM;

	/* register block device */
	ret = register_blkdev(DEV_FLOPPY_MAJOR, "fd", &floppy_fops);
	if (ret) {
		free_page(dma_base);
		return ret;
	}

	/* set request function */
	blk_dev[DEV_FLOPPY_MAJOR].request = fd_request;

	/* init block size */
	memset(&floppy_sizes, 0, sizeof(floppy_sizes));
	memset(&floppy_blksizes, 0, sizeof(floppy_blksizes));
	blk_size[DEV_FLOPPY_MAJOR] = floppy_sizes;
	blksize_size[DEV_FLOPPY_MAJOR] = floppy_blksizes;

	/* reset FDC */
	outb(FDC_DOR, 0x00);
	floppy_wait();

	/* activate DMA */
	outb(FDC_DOR, 0x0C);
	floppy_wait();

	/* send SPECIFY command */
	fdc_send_cmd(0x03);
	fdc_send_cmd(0xDF);
	fdc_send_cmd(0x00);

	/* detect drives via cmos */
	cmos_floppy = cmos_read(0x10);

	/* init drive 0 */
	if (cmos_floppy >> 4)
		init_floppy_drive(0);

	/* init drive 1 */
	if (cmos_floppy & 0x0F)
		init_floppy_drive(1);

	return 0;
}
