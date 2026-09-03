#include <lib/scatterlist.h>
#include <mm/paging.h>
#include <string.h>
#include <stdio.h>

/*
 * Get page of an sg entry.
 */
static struct page *sg_page(struct scatterlist *sg)
{
	if (sg->sg_magic != SG_MAGIC || sg_is_chain(sg))
		panic("sg_page: bad scatterlist\n");

	return (struct page *) ((sg)->page_link & ~0x3);
}

/*
 * Return physical address of an sg entry.
 */
uint32_t sg_phys(struct scatterlist *sg)
{
	return page_to_phys(sg_page(sg)) + sg->offset;
}

/*
 *  Assign a given page to an sg entry.
 */
static void sg_assign_page(struct scatterlist *sg, struct page *page)
{
	uint32_t page_link = sg->page_link & 0x3;

	/* sanity check */
	if ((uint32_t) page & 0x03)
		panic("sg_assign_page: page is not aligned\n");
	if (sg->sg_magic != SG_MAGIC || sg_is_chain(sg))
		panic("sg_assign_page: bad scatterlist\n");

	/* assign page */
	sg->page_link = page_link | (uint32_t) page;
}

/*
 * Set sg entry to point at given page.
 */
static void sg_set_page(struct scatterlist *sg, struct page *page, size_t len, uint32_t offset)
{
	sg_assign_page(sg, page);
	sg->offset = offset;
	sg->length = len;
}

/*
 * Set sg entry to point at given data.
 */
void sg_set_buf(struct scatterlist *sg, const void *buf, size_t buf_len)
{
	sg_set_page(sg, virt_to_page(buf), buf_len, offset_in_page(buf));
}

/*
 * Mark the end of the scatterlist.
 */
static void sg_mark_end(struct scatterlist *sg)
{
	if (sg->sg_magic != SG_MAGIC)
		panic("sg_mark_end: bad scatterlist\n");

	/* set termination bit, clear potential chain bit */
	sg->page_link |= 0x02;
	sg->page_link &= ~0x01;
}

/*
 * Initialize sg table.
 */
void sg_init_table(struct scatterlist *sgl, size_t nents)
{
	size_t i;

	/* clear table */
	memset(sgl, 0, sizeof(struct scatterlist) * nents);

	/* set magic */
	for (i = 0; i < nents; i++)
		sgl[i].sg_magic = SG_MAGIC;

	/* mark end */
	sg_mark_end(&sgl[nents - 1]);
}

/*
 * Initialize a single entry sg list.
 */
void sg_init_one(struct scatterlist *sg, const void *buf, size_t buf_len)
{
	sg_init_table(sg, 1);
	sg_set_buf(sg, buf, buf_len);
}
