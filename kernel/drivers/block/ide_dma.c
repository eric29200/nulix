#include <drivers/block/ide.h>
#include <x86/io.h>
#include <stdio.h>
#include <stderr.h>

/*
 * Setup dma.
 */
int ide_setup_dma(struct ide_drive *drive)
{
	/* set dma base address */
	drive->hwif->dma_base = drive->hwif->pci_dev->bar[4] & PCI_BASE_ADDRESS_IO_MASK;

	/* allocate scatter list */
	drive->sg_table = (struct scatterlist *) kmalloc(sizeof(struct scatterlist) * PRD_ENTRIES);
	if (!drive->sg_table)
		return -ENOMEM;

	/* allocate dma table */
	drive->dma_table = get_free_pages(get_order(PRD_ENTRIES * PRD_BYTES));
	if (!drive->dma_table) {
		kfree(drive->sg_table);
		return -ENOMEM;
	}

	return 0;
}

/*
 * Build a scatter list with for dma.
 */
static int ide_build_sglist(struct ide_drive *drive, struct request *req)
{
	struct scatterlist *sg = drive->sg_table;
	uint32_t last_data_end = ~0UL;
	struct buffer_head *bh;
	struct list_head *pos;
	int nents = 0;

	/* add each buffer */
	list_for_each(pos, &req->bhs_list) {
		bh = list_entry(pos, struct buffer_head, b_list_req);

		/* merge with previous entry if possible */
		if ((uint32_t) bh->b_data == last_data_end) {
			sg[nents - 1].length += bh->b_size;
			last_data_end += bh->b_size;
			continue;
		}

		/* scatter list too small */
		if (nents >= PRD_ENTRIES)
			return 0;

		/* add buffer */
		memset(&sg[nents], 0, sizeof(struct scatterlist));
		sg[nents].address = bh->b_data;
		sg[nents].length = bh->b_size;
		nents++;

		/* save last data address */
		last_data_end = (uint32_t) bh->b_data + bh->b_size;
	}

	return nents;
}

/*
 * Build dma table.
 */
static int ide_build_dmatable(struct ide_drive *drive, struct request *req)
{
	uint32_t *table = drive->dma_table, cur_addr, cur_len, bcount;
	struct scatterlist *sg;
	int nents, count = 0;

	/* build scatter list */
	nents = ide_build_sglist(drive, req);
	if (!nents)
		return 0;

	/* build dma table, without crossing any 64kB boundaries */
	for (sg = drive->sg_table; sg->length; sg++, nents--) {
		cur_addr = __pa(sg->address);
		cur_len = sg->length;

		while (cur_len) {
			if (count++ >= PRD_ENTRIES)
				return 0;

			/* limit dma entry to 64kB */
			bcount = 0x10000 - (cur_addr & 0xFFFF);
			if (bcount > cur_len)
				bcount = cur_len;

			/* set dma entry */
			*table++ = cur_addr;
			*table++ = bcount & 0xFFFF;

			cur_addr += bcount;
			cur_len -= bcount;
		}
	}

	/* end last entry */
	if (count)
		*--table |= 0x80000000;

	return count;
}

/*
 * Issue a DMA command.
 */
int ide_dmaproc(struct ide_drive *drive, struct request *req)
{
	uint32_t dma_base = drive->hwif->dma_base;

	/* build dma table */
	if (!ide_build_dmatable(drive, req))
		return 1;

	/* prepare DMA transfert */
	outb(dma_base, 0);
	outl(dma_base + 0x04, __pa(drive->dma_table));
	outb(dma_base + 0x02, inb(dma_base + 0x02) | 0x06);
	outb(dma_base, (req->cmd == READ ? 0x08 : 0x00) | 0x01);

	/* issue command */
	outb(drive->io_base + ATA_REG_COMMAND, req->cmd == READ ? ATA_CMD_READ_DMA : ATA_CMD_WRITE_DMA);

	return 0;
}