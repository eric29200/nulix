#ifndef _SCATTERLIST_H_
#define _SCATTERLIST_H_

#include <stddef.h>

#define SG_MAGIC		0x87654321

#define sg_is_chain(sg)		((sg)->page_link & 0x01)

/*
 * Scatter list = list of non contiguous buffers.
 */
struct scatterlist {
	uint32_t		sg_magic;
	uint32_t		page_link;
	uint32_t		offset;
	size_t			length;
};

void sg_init_one(struct scatterlist *sg, const void *buf, size_t buf_len);
void sg_init_table(struct scatterlist *sgl, size_t nents);
void sg_set_buf(struct scatterlist *sg, const void *buf, size_t buf_len);
uint32_t sg_phys(struct scatterlist *sg);

#endif
