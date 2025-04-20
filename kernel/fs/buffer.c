#include <fs/fs.h>
#include <proc/sched.h>
#include <proc/timer.h>
#include <drivers/block/ata.h>
#include <mm/mm.h>
#include <string.h>
#include <stderr.h>
#include <stdio.h>
#include <time.h>
#include <dev.h>

#define NR_BUFFERS_MAX			65521
#define NR_SIZES			4
#define BUFSIZE_INDEX(size)		(buffersize_index[(size) >> 9])

#define HASH_BITS			12
#define HASH_SIZE			(1 << HASH_BITS)

/* global buffer table */
static int nr_buffers = 0;
static struct htable_link **buffer_htable = NULL;

/* buffers lists */
static char buffersize_index[9] = { -1, 0, 1, -1, 2, -1, -1, -1, 3 };
static LIST_HEAD(unused_list);
static LIST_HEAD(used_list);
static struct list_head free_list[NR_SIZES];

/* block size of devices */
size_t *blocksize_size[MAX_BLKDEV] = { NULL, NULL };

/*
 * Set blocksize of a device.
 */
void set_blocksize(dev_t dev, size_t blocksize)
{
	/* check device */
	if (!blocksize_size[major(dev)])
		return;

	/* check block size */
	switch (blocksize) {
		case 512:
		case 1024:
		case 2048:
		case 4096:
			break;
		default:
			panic("set_blocksize : invalid block size");
	}

	/* nothing to do */
	if (blocksize_size[major(dev)][minor(dev)] == blocksize)
		return;

 	/* sync buffers */
	bsync();

	/* set block size */
	blocksize_size[major(dev)][minor(dev)] = blocksize;
}

/*
 * Grow buffers list.
 */
static int grow_buffers()
{
	struct buffer_head *bhs;
	int i, n;

	/* get some memory */
	bhs = (struct buffer_head *) get_free_page(GFP_KERNEL);
	if (!bhs)
		return -ENOMEM;
	memset(bhs, 0, PAGE_SIZE);

	/* update number of buffers */
	n = PAGE_SIZE / sizeof(struct buffer_head);
	nr_buffers += n;

	/* add buffers to unused list */
	for (i = 0; i < n; i++)
		list_add(&bhs[i].b_list, &unused_list);

	return 0;
}

/*
 * Get an unused buffer.
 */
static struct buffer_head *get_unused_buffer()
{
	struct buffer_head *bh;

	/* grow buffers if needed */
	if (list_empty(&unused_list) && nr_buffers < NR_BUFFERS_MAX)
		grow_buffers();

	/* no more unused buffers */
	if (list_empty(&unused_list)) {
		/* reclaim pages */
		reclaim_pages();

		/* recheck unused list */
		if (list_empty(&unused_list))
			return NULL;
	}

	/* get first free unused buffer */
	bh = list_first_entry(&unused_list, struct buffer_head, b_list);
	list_del(&bh->b_list);
	list_add(&bh->b_list, &used_list);

	return bh;
}

/*
 * Put a buffer in unused list.
 */ 
static void put_unused_buffer(struct buffer_head *bh)
{
	list_del(&bh->b_list);
	memset(bh, 0, sizeof(struct buffer_head));
	list_add(&bh->b_list, &unused_list);
}

/*
 * Create new buffers.
 */
static struct buffer_head *create_buffers(void *page, size_t size)
{
	struct buffer_head *bh, *head, *tail;
	int offset;

	/* create buffers */
	head = NULL;
	tail = NULL;
	for (offset = PAGE_SIZE - size; offset >= 0; offset -= size) {
		/* get an unused buffer */
		bh = get_unused_buffer();
		if (!bh)
			goto err;

		/* set buffer */
		bh->b_data = (char *) (page + offset);
		bh->b_size = size;
		bh->b_this_page = head;

		/* set tail and head */
		if (!head)
			tail = bh;
		head = bh;
	}

	/* end circular list */
	if (tail)
		tail->b_this_page = head;

	return head;
err:
	/* put buffers in unused list */
	for (bh = head; bh != NULL;) {
		head = bh;
		bh = bh->b_this_page;
		put_unused_buffer(head);
	}

	return NULL;
}

/*
 * Refill free list.
 */
static int refill_freelist(size_t blocksize)
{
	size_t isize = BUFSIZE_INDEX(blocksize);
	struct buffer_head *bh, *tmp;
	void *page;

	/* get a new page */
	page = get_free_page();
	if (!page)
		return -ENOMEM;

	/* create new buffers */
	bh = create_buffers(page, blocksize);
	if (!bh) {
		free_page(page);
		return -ENOMEM;
	}

	/* add new buffers to free list */
	tmp = bh;
	do {
		list_del(&tmp->b_list);
		list_add_tail(&tmp->b_list, &free_list[isize]);
		tmp = tmp->b_this_page;
	} while (tmp != bh);

	/* set page buffers */
	page_array[MAP_NR((uint32_t) page)].buffers = bh;

	return 0;
}

/*
 * Find a buffer in hash table.
 */
static struct buffer_head *find_buffer(dev_t dev, uint32_t block, size_t blocksize)
{
	struct htable_link *node;
	struct buffer_head *bh;

	/* try to find buffer in cache */
	node = htable_lookup(buffer_htable, block, HASH_BITS);
	while (node) {
		bh = htable_entry(node, struct buffer_head, b_htable);
		if (bh->b_block == block && bh->b_dev == dev && bh->b_size == blocksize) {
			bh->b_ref++;
			return bh;
		}

		node = node->next;
	}

	return NULL;
}

/*
 * Get a buffer (from cache or create one).
 */
struct buffer_head *getblk(dev_t dev, uint32_t block, size_t blocksize)
{
	size_t isize = BUFSIZE_INDEX(blocksize);
	struct buffer_head *bh;

	/* try to find buffer in cache */
	bh = find_buffer(dev, block, blocksize);
	if (bh)
		return bh;

	/* refill free list if needed */
	if (list_empty(&free_list[isize])) {
		refill_freelist(blocksize);

		/* recheck free list */
		if (list_empty(&free_list[isize]))
			return NULL;
	}

	/* get first free buffer */
	bh = list_first_entry(&free_list[isize], struct buffer_head, b_list);
	list_del(&bh->b_list);
	list_add(&bh->b_list, &used_list);

	/* set buffer */
	bh->b_ref = 1;
	bh->b_dev = dev;
	bh->b_block = block;
	bh->b_uptodate = 0;

	/* hash the new buffer */
	htable_delete(&bh->b_htable);
	htable_insert(buffer_htable, &bh->b_htable, block, HASH_BITS);

	return bh;
}

/*
 * Read a block from a device.
 */
struct buffer_head *bread(dev_t dev, uint32_t block, size_t blocksize)
{
	struct buffer_head *bh;

	/* get buffer */
	bh = getblk(dev, block, blocksize);
	if (!bh)
		return NULL;

	/* read it from device */
	if (!bh->b_uptodate && block_read(bh) != 0) {
		brelse(bh);
		return NULL;
	}

	bh->b_uptodate = 1;
	return bh;
}

/*
 * Write a block buffer.
 */
int bwrite(struct buffer_head *bh)
{
	int ret;

	if (!bh)
		return -EINVAL;

	/* write to block device */
	ret = block_write(bh);
	if (ret)
		return ret;

	bh->b_dirt = 0;
	return ret;
}

/*
 * Release a buffer.
 */
void brelse(struct buffer_head *bh)
{
	if (!bh)
		return;

	/* write dirty buffer */
	if (bh->b_dirt)
		bwrite(bh);

	/* update inode reference count */
	bh->b_ref--;
}

/*
 * Try to free a buffer.
 */
void try_to_free_buffer(struct buffer_head *bh)
{
	struct buffer_head *tmp, *tmp1;
	uint32_t page;

	/* already freed */
	if (!bh->b_this_page)
		return;

	/* get page address */
	page = (uint32_t) bh->b_data & PAGE_MASK;

	/* check if all page buffers can be freed */
	tmp = bh;
	do {
		/* used buffer */
		if (tmp->b_ref || tmp->b_dirt)
			return;

		/* go to next buffer in page */
		tmp = tmp->b_this_page;
	} while (tmp != bh);

	/* ok to free buffers */
	tmp = bh;
	do {
		/* save next buffer */
		tmp1 = tmp->b_this_page;

		/* remove it from lists */
		htable_delete(&tmp->b_htable);
		put_unused_buffer(tmp);

		/* go to next buffer in page */
		tmp = tmp1;
	} while (tmp != bh);

	/* free page */
	free_page((void *) page);
}

/*
 * Write all dirty buffers of a list on disk.
 */
static void bsync_list(struct list_head *head, dev_t dev)
{
	struct buffer_head *bh;
	struct list_head *pos;

	list_for_each(pos, head) {
		bh = (struct buffer_head *) list_entry(pos, struct buffer_head, b_list);

		if (!bh->b_dirt || (dev && bh->b_dev != dev))
			continue;

		if (bwrite(bh)) {
			printf("Can't write block %d on disk\n", bh->b_block);
			panic("Disk error");
		}
	}
}

/*
 * Write all dirty buffers on disk.
 */
void bsync()
{
	int i;

	bsync_list(&used_list, 0);
	bsync_list(&unused_list, 0);
	for (i = 0; i < NR_SIZES; i++)
		bsync_list(&free_list[i], 0);
}

/*
 * Write all dirty buffers on disk.
 */
void bsync_dev(dev_t dev)
{
	int i;

	bsync_list(&used_list, dev);
	bsync_list(&unused_list, dev);
	for (i = 0; i < NR_SIZES; i++)
		bsync_list(&free_list[i], dev);
}

/*
 * Sync system call.
 */
int sys_sync()
{
	/* sync all buffers */
	bsync();

	return 0;
}

/*
 * Fsync system call (don't do anything because all block buffers should be clean).
 */
int sys_fsync(int fd)
{
	UNUSED(fd);
	return 0;
}

/*
 * Read a page.
 */
int generic_readpage(struct inode *inode, struct page *page)
{
	struct super_block *sb = inode->i_sb;
	struct buffer_head *bh, *next, *tmp;
	uint32_t block, address;
	int nr, i;

	/* compute blocks to read */
	nr = PAGE_SIZE >> sb->s_blocksize_bits;
	block = page->offset >> sb->s_blocksize_bits;
	address = PAGE_ADDRESS(page);

	/* create temporary buffers */
	bh = create_buffers((void *) address, sb->s_blocksize);
	if (!bh)
		return -ENOMEM;

	/* read block by block */
	for (i = 0, next = bh; i < nr; i++, block++) {
		/* set block buffer */
		next->b_dev = sb->s_dev;
		next->b_block = inode->i_op->bmap(inode, block);
		next->b_uptodate = 1;

		/* check if buffer is already hashed */
		tmp = find_buffer(sb->s_dev, next->b_block, sb->s_blocksize);
		if (tmp) {
			/* read it from disk if needed */
			if (!tmp->b_uptodate)
				block_read(tmp);

			/* copy data to user address space */
			memcpy(next->b_data, tmp->b_data, sb->s_blocksize);

			/* release buffer */
			brelse(tmp);
			goto next;
		}

		/* read buffer on disk */
		block_read(next);
 next:
		/* clear temporary buffer */
		tmp = next->b_this_page;
		put_unused_buffer(next);
		next = tmp;
	}

	return 0;
}

/*
 * Init buffers.
 */
int init_buffer()
{
	int i;

	/* init buffers list */
	for (i = 0; i < NR_SIZES; i++)
		INIT_LIST_HEAD(&free_list[i]);

	/* allocate buffers hash table */
	buffer_htable = (struct htable_link **) kmalloc(sizeof(struct htable_link *) * HASH_SIZE);
	if (!buffer_htable)
		return -ENOMEM;
	memset(buffer_htable, 0, sizeof(struct htable_link *) * HASH_SIZE);

	/* init buffers hash table */
	htable_init(buffer_htable, HASH_BITS);

	return 0;
}
