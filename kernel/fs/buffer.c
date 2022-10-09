#include <fs/fs.h>
#include <proc/sched.h>
#include <proc/timer.h>
#include <drivers/ata.h>
#include <mm/mm.h>
#include <string.h>
#include <stderr.h>
#include <stdio.h>
#include <time.h>

/* global buffer table */
static struct buffer_head_t *buffer_table = NULL;
static struct htable_link_t **buffer_htable = NULL;
static LIST_HEAD(lru_buffers);

/*
 * Write a block buffer.
 */
int bwrite(struct buffer_head_t *bh)
{
	int ret;

	if (!bh)
		return -EINVAL;

	/* write to block device */
	ret = ata_write(bh->b_sb->s_dev, bh);
	if (ret)
		return ret;

	bh->b_dirt = 0;
	return ret;
}

/*
 * Get an empty buffer.
 */
static struct buffer_head_t *get_empty_buffer(struct super_block_t *sb)
{
	struct buffer_head_t *bh;
	struct list_head_t *pos;

	/* get first free entry from LRU list */
	list_for_each(pos, &lru_buffers) {
		bh = list_entry(pos, struct buffer_head_t, b_list);
		if (!bh->b_ref)
			goto found;
	}

	/* no free buffer : exit */
	return NULL;

found:
	/* write it on disk if needed */
	if (bh->b_dirt && bwrite(bh))
		printf("Can't write block %d on disk\n", bh->b_block);

	/* free data if found block is too small */
	if (bh->b_size && bh->b_size < sb->s_blocksize) {
		kfree(bh->b_data);
		bh->b_size = 0;
	}

	/* allocate data if needed */
	if (!bh->b_size) {
		bh->b_data = (char *) kmalloc(sb->s_blocksize);
		if (!bh->b_data)
			return NULL;

		bh->b_size = sb->s_blocksize;
	}

	/* reset buffer */
	bh->b_ref = 1;
	bh->b_sb = sb;
	memset(bh->b_data, 0, bh->b_size);

	return bh;
}

/*
 * Get a buffer (from cache or create one).
 */
struct buffer_head_t *getblk(struct super_block_t *sb, uint32_t block)
{
	struct htable_link_t *node;
	struct buffer_head_t *bh;

	/* try to find buffer in cache */
	node = htable_lookup(buffer_htable, block, BUFFER_HTABLE_BITS);
	while (node) {
		bh = htable_entry(node, struct buffer_head_t, b_htable);
		if (bh->b_block == block && bh->b_sb == sb && bh->b_size == sb->s_blocksize) {
			bh->b_ref++;
			goto out;
		}

		node = node->next;
	}

	/* get an empty buffer */
	bh = get_empty_buffer(sb);
	if (!bh)
		return NULL;

	/* set buffer */
	bh->b_sb = sb;
	bh->b_block = block;
	bh->b_uptodate = 0;

	/* hash the new buffer */
	htable_delete(&bh->b_htable);
	htable_insert(buffer_htable, &bh->b_htable, block, BUFFER_HTABLE_BITS);
out:
	/* put it at the end of LRU list */
	list_del(&bh->b_list);
	list_add_tail(&bh->b_list, &lru_buffers);
	return bh;
}

/*
 * Read a block from a device.
 */
struct buffer_head_t *bread(struct super_block_t *sb, uint32_t block)
{
	struct buffer_head_t *bh;

	/* get buffer */
	bh = getblk(sb, block);
	if (!bh)
		return NULL;

	/* read it from device */
	if (!bh->b_uptodate && ata_read(sb->s_dev, bh) != 0) {
		brelse(bh);
		return NULL;
	}

	bh->b_uptodate = 1;
	return bh;
}

/*
 * Release a buffer.
 */
void brelse(struct buffer_head_t *bh)
{
	if (!bh)
		return;

	/* update inode reference count */
	bh->b_ref--;
}

/*
 * Write all dirty buffers on disk.
 */
void bsync()
{
	int i;

	/* write all dirty buffers */
	for (i = 0; i < NR_BUFFER; i++) {
		if (buffer_table[i].b_dirt && bwrite(&buffer_table[i])) {
			printf("Can't write block %d on disk\n", buffer_table[i].b_block);
			panic("Disk error");
		}
	}
}

/*
 * Init buffers.
 */
int binit()
{
	int i;

	/* allocate buffers */
	buffer_table = (struct buffer_head_t *) kmalloc(sizeof(struct buffer_head_t) * NR_BUFFER);
	if (!buffer_table)
		return -ENOMEM;

	/* memzero all buffers */
	memset(buffer_table, 0, sizeof(struct buffer_head_t) * NR_BUFFER);

	/* init Last Recently Used buffers list */
	INIT_LIST_HEAD(&lru_buffers);

	/* add all buffers to LRU list */
	for (i = 0; i < NR_BUFFER; i++)
		list_add(&buffer_table[i].b_list, &lru_buffers);

	/* allocate buffers hash table */
	buffer_htable = (struct htable_link_t **) kmalloc(sizeof(struct htable_link_t *) * NR_BUFFER);
	if (!buffer_htable) {
		kfree(buffer_table);
		return -ENOMEM;
	}

	/* init buffers hash table */
	htable_init(buffer_htable, BUFFER_HTABLE_BITS);

	return 0;
}
