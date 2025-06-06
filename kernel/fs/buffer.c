#include <fs/fs.h>
#include <proc/sched.h>
#include <proc/timer.h>
#include <drivers/block/ata.h>
#include <drivers/block/blk_dev.h>
#include <mm/mm.h>
#include <mm/highmem.h>
#include <string.h>
#include <stderr.h>
#include <stdio.h>
#include <time.h>
#include <dev.h>

#define NR_BUFFERS_MAX			65521
#define MAX_UNUSED_BUFFERS		32
#define NR_SIZES			4
#define BUFSIZE_INDEX(size)		(buffersize_index[(size) >> 9])
#define NBUF				32

#define HASH_BITS			12
#define HASH_SIZE			(1 << HASH_BITS)

/* global buffer table */
static int nr_buffers = 0;
static int nr_buffer_heads = 0;
static int nr_unused_buffer_heads = 0;
uint32_t buffermem_pages = 0;
static struct buffer_head *buffer_hash_table[HASH_SIZE];

/* buffers lists */
static char buffersize_index[9] = { -1, 0, 1, -1, 2, -1, -1, -1, 3 };
static LIST_HEAD(unused_list);
static LIST_HEAD(used_list);
static LIST_HEAD(dirty_list);
static struct list_head free_list[NR_SIZES];

/* block size of devices */
size_t *blocksize_size[MAX_BLKDEV] = { NULL, NULL };

/*
 * Is a buffer dirty ?
 */
int buffer_dirty(struct buffer_head *bh)
{
	return bh->b_dirt == 1;
}

/*
 * Is a buffer up to date ?
 */
int buffer_uptodate(struct buffer_head *bh)
{
	return bh->b_uptodate == 1;
}

/*
 * Mark a buffer clean.
 */
void mark_buffer_clean(struct buffer_head *bh)
{
	if (bh->b_dirt) {
		bh->b_dirt = 0;
		list_del(&bh->b_list);
		list_add(&bh->b_list, &used_list);
	}
}

/*
 * Mark a buffer dirty.
 */
void mark_buffer_dirty(struct buffer_head *bh)
{
	struct buffer_head *next = NULL;
	struct list_head *pos;

	if (!bh->b_dirt) {
		bh->b_dirt = 1;
		list_del(&bh->b_list);

		/* keep dirty list sorted */
		list_for_each(pos, &dirty_list) {
			next = list_entry(pos, struct buffer_head, b_list);

			if (next->b_dev > bh->b_dev)
				break;
			if (next->b_block == bh->b_block && next->b_block > bh->b_block)
				break;
		}

		if (next)
			list_add_tail(&bh->b_list, &next->b_list);
		else
			list_add(&bh->b_list, &dirty_list);
	}
}

/*
 * Mark a buffer up to date.
 */
void mark_buffer_uptodate(struct buffer_head *bh, int on)
{
	bh->b_uptodate = on;
}

/*
 * Hash a buffer.
 */
static inline int __buffer_hashfn(dev_t dev, uint32_t block)
{
	return (dev ^ block) % HASH_SIZE;
}

/*
 * Get hash bucket.
 */
static inline struct buffer_head **buffer_hash(dev_t dev, uint32_t block)
{
	return buffer_hash_table + __buffer_hashfn(dev, block);
}

/*
 * Insert a buffer in hash table.
 */
static void add_to_buffer_cache(struct buffer_head *bh)
{
	struct buffer_head **b;

	b = buffer_hash(bh->b_dev, bh->b_block);
	bh->b_prev_hash = NULL;
	bh->b_next_hash = *b;

	if (*b)
		bh->b_next_hash->b_prev_hash = bh;

	/* update head */
	*b = bh;
}

/*
 * Remove a buffer from cache.
 */
static void remove_from_buffer_cache(struct buffer_head *bh)
{
	struct buffer_head *next_hash = bh->b_next_hash, *prev_hash = bh->b_prev_hash;
	struct buffer_head **b;

	/* unlink this buffer */
	bh->b_next_hash = NULL;
	bh->b_prev_hash = NULL;

	/* update next */
	if (next_hash)
		next_hash->b_prev_hash = prev_hash;

	/* update previous */
	if (prev_hash)
		prev_hash->b_next_hash = next_hash;

	/* update head */
	b = buffer_hash(bh->b_dev, bh->b_block);
	if (*b == bh)
		*b = next_hash;
}

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
 * Put a buffer in unused list.
 */
static void put_unused_buffer_head(struct buffer_head *bh)
{
	/* free buffer */
	if (nr_unused_buffer_heads >= MAX_UNUSED_BUFFERS) {
		nr_buffer_heads--;
		list_del(&bh->b_list);
		kfree(bh);
		return;
	}

	/* or add it to unused list */
	list_del(&bh->b_list);
	memset(bh, 0, sizeof(struct buffer_head));
	list_add(&bh->b_list, &unused_list);
	nr_unused_buffer_heads++;
}

/*
 * Get an unused buffer.
 */
static struct buffer_head *get_unused_buffer_head()
{
	struct buffer_head *bh;

	/* try to reuse a buffer */
	if (!list_empty(&unused_list)) {
		bh = list_first_entry(&unused_list, struct buffer_head, b_list);
		list_del(&bh->b_list);
		list_add(&bh->b_list, &used_list);
		nr_unused_buffer_heads--;

		return bh;
	}

	/* allocate a new buffer */
	bh = (struct buffer_head *) kmalloc(sizeof(struct buffer_head));
	if (!bh)
		return NULL;

	/* set new buffer */
	memset(bh, 0, sizeof(struct buffer_head));
	list_add(&bh->b_list, &used_list);
	nr_buffer_heads++;

	return bh;
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
		bh = get_unused_buffer_head();
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
		put_unused_buffer_head(head);
	}

	return NULL;
}

/*
 * Grow buffers list.
 */
static int grow_buffers(size_t size)
{
	struct buffer_head *bh, *tmp;
	size_t isize = BUFSIZE_INDEX(size);
	void *page;

	/* get a page */
	page = get_free_page();
	if (!page)
		return -ENOMEM;

	/* create buffers */
	bh = create_buffers(page, size);
	if (!bh) {
		free_page(page);
		return -ENOMEM;
	}

	/* add new buffers to free list */
	tmp = bh;
	do {
		list_del(&tmp->b_list);
		list_add_tail(&tmp->b_list, &free_list[isize]);
		nr_buffers++;
		tmp = tmp->b_this_page;
	} while (tmp != bh);

	/* set page buffers */
	page_array[MAP_NR((uint32_t) page)].buffers = bh;
	buffermem_pages++;

	return 0;
}

/*
 * Refill free list.
 */
static void refill_freelist(size_t blocksize)
{
	grow_buffers(blocksize);
}

/*
 * Find a buffer in hash table.
 */
struct buffer_head *find_buffer(dev_t dev, uint32_t block, size_t blocksize)
{
	struct buffer_head *bh;

	for (bh = *buffer_hash(dev, block); bh != NULL; bh = bh->b_next_hash) {
		if (bh->b_block == block && bh->b_dev == dev && bh->b_size == blocksize) {
			bh->b_count++;
			return bh;
		}
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
	bh->b_count = 1;
	bh->b_dev = dev;
	bh->b_block = block;
	mark_buffer_uptodate(bh, 0);

	/* cache the new buffer */
	add_to_buffer_cache(bh);

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

	/* buffer up to date */
	if (buffer_uptodate(bh))
		return bh;

	/* read it from device */
	ll_rw_block(READ, 1, &bh);
	execute_block_requests();

	return bh;
}

/*
 * Release a buffer.
 */
void brelse(struct buffer_head *bh)
{
	if (!bh)
		return;

	/* update buffer reference count */
	bh->b_count--;
}

/*
 * Try to free a buffer.
 */
void try_to_free_buffer(struct buffer_head *bh)
{
	struct buffer_head *tmp, *tmp1;
	uint32_t page;

	/* get page */
	page = (uint32_t) bh->b_data;
	page &= PAGE_MASK;

	/* check if all page buffers can be freed */
	tmp = bh;
	do {
		/* used buffer */
		if (tmp->b_count || buffer_dirty(tmp))
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
		remove_from_buffer_cache(tmp);
		put_unused_buffer_head(tmp);
		nr_buffers--;

		/* go to next buffer in page */
		tmp = tmp1;
	} while (tmp != bh);

	/* free page */
	buffermem_pages--;
	page_array[MAP_NR(page)].buffers = NULL;
	free_page((void *) page);
}

/*
 * Write all dirty buffers on disk.
 */
static void __bsync(dev_t dev)
{
	struct buffer_head *bh, *bhs_list[NBUF];
	struct list_head *pos, *n;
	size_t bhs_count = 0;

	list_for_each_safe(pos, n, &dirty_list) {
		bh = (struct buffer_head *) list_entry(pos, struct buffer_head, b_list);

		/* clean buffer */
		if (!buffer_dirty(bh) || (dev && bh->b_dev != dev))
			continue;

		/* write buffers */
		if (bhs_count == NBUF) {
			ll_rw_block(WRITE, bhs_count, bhs_list);
			execute_block_requests();
			bhs_count = 0;
		}

		/* add buffer */
		bhs_list[bhs_count++] = bh;
	}

	/* write last buffers */
	if (bhs_count) {
		ll_rw_block(WRITE, bhs_count, bhs_list);
		execute_block_requests();
	}
}

/*
 * Write all dirty buffers on disk.
 */
void bsync()
{
	__bsync(0);
}

/*
 * Write all dirty buffers on disk.
 */
void bsync_dev(dev_t dev)
{
	__bsync(dev);
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
 * Generic block map.
 */
static int generic_block_bmap(struct inode *inode, uint32_t block)
{
	struct buffer_head bh;

	if (inode->i_op->get_block(inode, block, &bh, 0))
		return 0;

	return bh.b_block;
}

/*
 * Read a page.
 */
int generic_readpage(struct inode *inode, struct page *page)
{
	struct super_block *sb = inode->i_sb;
	struct buffer_head *bh, *next, *tmp;
	void *page_address;
	int nr, i, ret = 0;
	uint32_t block;

	/* compute blocks to read */
	nr = PAGE_SIZE >> sb->s_blocksize_bits;
	block = page->offset >> sb->s_blocksize_bits;

	/* get page address */
	page_address = page_address(page);

	/* create temporary buffers */
	bh = create_buffers(page_address, sb->s_blocksize);
	if (!bh) {
		ret = -ENOMEM;
		goto out;
	}

	/* read block by block */
	for (i = 0, next = bh; i < nr; i++, block++) {
		/* set block buffer */
		next->b_dev = sb->s_dev;
		next->b_block = generic_block_bmap(inode, block);
		mark_buffer_uptodate(next, 1);

		/* check if buffer is already hashed */
		tmp = find_buffer(sb->s_dev, next->b_block, sb->s_blocksize);
		if (tmp) {
			/* read it from disk if needed */
			if (!buffer_uptodate(tmp)) {
			 	ll_rw_block(READ, 1, &tmp);
				execute_block_requests();
			}

			/* copy data to user address space */
			memcpy(next->b_data, tmp->b_data, sb->s_blocksize);

			/* release buffer */
			brelse(tmp);
			goto next;
		}

		/* read buffer on disk */
		ll_rw_block(READ, 1, &next);
 next:
		/* clear temporary buffer */
		tmp = next->b_this_page;
		put_unused_buffer_head(next);
		next = tmp;
	}

out:
	return ret;
}

/*
 * Init buffers.
 */
void init_buffer()
{
	int i;

	/* init buffers list */
	for (i = 0; i < NR_SIZES; i++)
		INIT_LIST_HEAD(&free_list[i]);

	/* init hash table */
	for (i = 0; i < HASH_SIZE; i++)
		buffer_hash_table[i] = NULL;
}
